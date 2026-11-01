/* macOsGrabber.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#include <base/HyperHdrInstance.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include <grabber/osx/macOS/macOsGrabber.h>

#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>

#ifdef MACOS_SCK
	#import <ScreenCaptureKit/ScreenCaptureKit.h>
	#import <CoreMedia/CoreMedia.h>
	#import <CoreVideo/CoreVideo.h>
	#include <mutex>
	#include <algorithm>

	@class ScreenStreamOutput;
#endif

namespace
{
	id activity;
	#ifdef MACOS_SCK
		ScreenStreamOutput* streamOutput = nil;
	#endif
};

#ifdef MACOS_SCK

// Receives frames from the persistent SCStream on its own dispatch queue and keeps only the latest complete one.
// The owning grabber is detached (set to nullptr) under the lock before it stops the stream or gets destroyed.
@interface ScreenStreamOutput : NSObject<SCStreamOutput, SCStreamDelegate>
{
@public
	std::mutex			_lock;
	macOsGrabber*		_grabber;
	quint64				_generation;
	SCStream*			_stream;
	CVPixelBufferRef	_latestFrame;
	dispatch_queue_t	_sequencer;
}
- (CVPixelBufferRef)copyLatestFrame;
- (void)reportFailure:(NSString*)message;
@end

@implementation ScreenStreamOutput

- (instancetype)init
{
	self = [super init];
	if (self)
	{
		_grabber = nullptr;
		_generation = 0;
		_stream = nil;
		_latestFrame = nullptr;
		_sequencer = dispatch_queue_create("HyperHDR.macOsGrabber", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0));
	}
	return self;
}

- (void)dealloc
{
	if (_latestFrame != nullptr)
		CVPixelBufferRelease(_latestFrame);
	[_stream release];
	dispatch_release(_sequencer);
	[super dealloc];
}

- (void)stream:(SCStream*)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type
{
	if (type != SCStreamOutputTypeScreen || !CMSampleBufferIsValid(sampleBuffer))
		return;

	// skip idle/blank/suspended notifications: they carry no new image
	CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
	if (attachments == nullptr || CFArrayGetCount(attachments) < 1)
		return;

	NSDictionary* info = (NSDictionary*)CFArrayGetValueAtIndex(attachments, 0);
	NSNumber* status = info[SCStreamFrameInfoStatus];
	if (status == nil || status.integerValue != SCFrameStatusComplete)
		return;

	CVPixelBufferRef frame = CMSampleBufferGetImageBuffer(sampleBuffer);
	if (frame == nullptr)
		return;

	CVPixelBufferRetain(frame);

	CVPixelBufferRef previous;
	{
		std::lock_guard<std::mutex> lock(_lock);
		previous = _latestFrame;
		_latestFrame = frame;
	}

	if (previous != nullptr)
		CVPixelBufferRelease(previous);
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
	[self reportFailure:[NSString stringWithFormat:@"Capture stream stopped: %@", error.localizedDescription]];
}

- (CVPixelBufferRef)copyLatestFrame
{
	std::lock_guard<std::mutex> lock(_lock);
	if (_latestFrame != nullptr)
		CVPixelBufferRetain(_latestFrame);
	return _latestFrame;
}

- (void)reportFailure:(NSString*)message
{
	std::lock_guard<std::mutex> lock(_lock);
	if (_grabber != nullptr)
	{
		QMetaObject::invokeMethod(_grabber, "streamFailed", Qt::QueuedConnection,
			Q_ARG(quint64, _generation), Q_ARG(QString, QString::fromNSString(message)));
	}
}

@end

#endif


macOsGrabber::macOsGrabber(const QString& device, const QString& configurationPath)
	: Grabber(configurationPath, "MACOS_SYSTEM:" + device.left(14))
	, _configurationPath(configurationPath)
	, _actualDisplay(0)
	, _nativeCropLeft(0)
	, _nativeCropRight(0)
	, _nativeCropTop(0)
	, _nativeCropBottom(0)
	, _streamGeneration(0)
	, _streamNativeWidth(0)
	, _streamPointSize(CGSizeZero)
{
	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &macOsGrabber::grabFrame);

	// Refresh devices
	getDevices();

	//
	activity = [[NSProcessInfo processInfo]beginActivityWithOptions:NSActivityBackground reason : @"HyperHDR background activity"];
	[activity retain] ;
}

QString macOsGrabber::GetSharedLut()
{
	return "";
}

void macOsGrabber::loadLutFile(PixelFormat color)
{		
}

void macOsGrabber::setHdrToneMappingEnabled(int mode)
{
}

macOsGrabber::~macOsGrabber()
{
	uninit();

	// destroy core elements from contructor

	[[NSProcessInfo processInfo]endActivity:activity];
	[activity release];
}

void macOsGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{		
		stop();
		Debug(_log, "Uninit grabber: {:s}", (_deviceName));
	}

	stopStream();

	_actualDisplay = 0;

	_initialized = false;
}

bool macOsGrabber::init()
{
	Debug(_log, "init");


	if (!_initialized)
	{
		QString foundDevice = "";
		bool    autoDiscovery = (QString::compare(_deviceName, Grabber::AUTO_SETTING, Qt::CaseInsensitive) == 0);

		enumerateDevices(true);


		if (!autoDiscovery && !_deviceProperties.contains(_deviceName))
		{
			Debug(_log, "Device {:s} is not available. Changing to auto.", (_deviceName));
			autoDiscovery = true;
		}

		if (autoDiscovery)
		{
			Debug(_log, "Forcing auto discovery device");
			if (!_deviceProperties.isEmpty())
			{				
				foundDevice = _deviceProperties.firstKey();
				_deviceName = foundDevice;
				Debug(_log, "Auto discovery set to {:s}", (_deviceName));
			}
		}
		else
			foundDevice = _deviceName;

		if (foundDevice.isNull() || foundDevice.isEmpty() || !_deviceProperties.contains(foundDevice))
		{
			Error(_log, "Could not find any capture device");
			return false;
		}

		
		Info(_log, "*************************************************************************************************");
		Info(_log, "Starting macOS grabber. Selected: '{:s}' max width: {:d} ({:d}) @ {:d} fps", (foundDevice), _width, _height, _fps);
		Info(_log, "*************************************************************************************************");

		_actualDisplay = _deviceProperties[foundDevice].valid.first().display;
		_initialized = true;		
	}

	return _initialized;
}


void macOsGrabber::getDevices()
{
	enumerateDevices(false);
}

void macOsGrabber::enumerateDevices(bool silent)
{
	_deviceProperties.clear();

	CGDisplayCount    screenCount;
	CGDisplayCount    maxDisplays = 16;
	CGDirectDisplayID onlineDisplays[16];

	CGDisplayErr status = CGGetOnlineDisplayList(maxDisplays, onlineDisplays, &screenCount);

	if (status != kCGErrorSuccess)
	{
		Error(_log, "Could not find any display (error: {:d})", static_cast<int>(status));
		return;
	}

	for (CGDisplayCount i = 0; i < screenCount; i++)
	{
		CGDirectDisplayID dID = onlineDisplays[i];
		QString id = QString("Display id: %1").arg(dID);

		if (!silent)
			Info(_log, "Found display: {:s}", (id));

		DeviceProperties properties;
		DevicePropertiesItem dpi;

		dpi.display = dID;
		properties.valid.append(dpi);

		_deviceProperties.insert(id, properties);
	}	
}

bool macOsGrabber::start()
{
	try
	{		
		if (init())
		{
			startStream();
			_timer.setInterval(1000/_fps);
			_timer.start();
			Info(_log, "Started");
			return true;
		}
	}
	catch (std::exception& e)
	{
		Error(_log, "start failed ({:s})", e.what());
	}

	return false;
}

void macOsGrabber::stop()
{
	if (_initialized)
	{
		_timer.stop();
		stopStream();
		Info(_log, "Stopped");
	}
}

void macOsGrabber::startStream()
{
	#ifdef MACOS_SCK
		stopStream();

		CGDisplayModeRef displayReference = CGDisplayCopyDisplayMode(_actualDisplay);
		if (displayReference == nullptr)
		{
			QMetaObject::invokeMethod(this, "streamFailed", Qt::QueuedConnection,
				Q_ARG(quint64, _streamGeneration), Q_ARG(QString, QString("Could not read the display mode")));
			return;
		}
		size_t nativeWidth = CGDisplayModeGetPixelWidth(displayReference);
		size_t nativeHeight = CGDisplayModeGetPixelHeight(displayReference);
		CGDisplayModeRelease(displayReference);

		// let ScreenCaptureKit scale on the GPU to the configured max width instead of copying full native frames
		size_t outputWidth = nativeWidth;
		size_t outputHeight = nativeHeight;
		if (_width > 0 && static_cast<size_t>(_width) < nativeWidth)
		{
			outputWidth = static_cast<size_t>(_width);
			outputHeight = (nativeHeight * outputWidth + nativeWidth / 2) / nativeWidth;
		}
		outputWidth = std::max<size_t>(2, outputWidth & ~static_cast<size_t>(1));
		outputHeight = std::max<size_t>(2, outputHeight & ~static_cast<size_t>(1));

		_streamNativeWidth = nativeWidth;
		_streamPointSize = CGDisplayBounds(_actualDisplay).size;

		quint64 generation = ++_streamGeneration;
		CGDirectDisplayID displayId = _actualDisplay;
		int fps = std::max(1, _fps);

		streamOutput = [ScreenStreamOutput new];
		streamOutput->_grabber = this;
		streamOutput->_generation = generation;

		Debug(_log, "Creating capture stream {:d}x{:d} (native: {:d}x{:d}) @ {:d} fps", outputWidth, outputHeight, nativeWidth, nativeHeight, fps);

		ScreenStreamOutput* output = streamOutput;

		[SCShareableContent getShareableContentExcludingDesktopWindows:YES onScreenWindowsOnly:YES completionHandler:^(SCShareableContent* content, NSError* error)
		{
			if (error != nil)
			{
				[output reportFailure:[NSString stringWithFormat:@"Could not get shareable content (permission not granted?): %@", error.localizedDescription]];
				return;
			}

			SCDisplay* target = nil;
			for (SCDisplay* display in content.displays)
			{
				if (display.displayID == displayId)
				{
					target = display;
					break;
				}
			}

			if (target == nil)
			{
				[output reportFailure:@"Could not find the selected display"];
				return;
			}

			std::lock_guard<std::mutex> lock(output->_lock);

			// the grabber was stopped while we were waiting
			if (output->_grabber == nullptr)
				return;

			SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:target excludingWindows:@[]];
			SCStreamConfiguration* streamConfig = [[SCStreamConfiguration alloc] init];

			streamConfig.width = outputWidth;
			streamConfig.height = outputHeight;
			streamConfig.minimumFrameInterval = CMTimeMake(1, fps);
			streamConfig.pixelFormat = kCVPixelFormatType_32BGRA;
			streamConfig.queueDepth = 3;

			SCStream* stream = [[SCStream alloc] initWithFilter:filter configuration:streamConfig delegate:output];

			[streamConfig release];
			[filter release];

			NSError* outputError = nil;
			if (![stream addStreamOutput:output type:SCStreamOutputTypeScreen sampleHandlerQueue:output->_sequencer error:&outputError])
			{
				[stream release];
				QMetaObject::invokeMethod(output->_grabber, "streamFailed", Qt::QueuedConnection,
					Q_ARG(quint64, output->_generation), Q_ARG(QString, QString::fromNSString(outputError.localizedDescription)));
				return;
			}

			output->_stream = stream;

			[stream startCaptureWithCompletionHandler:^(NSError* startError)
			{
				if (startError != nil)
				{
					[output reportFailure:[NSString stringWithFormat:@"Could not start the capture stream: %@", startError.localizedDescription]];
				}
			}];
		}];
	#endif
}

void macOsGrabber::stopStream()
{
	#ifdef MACOS_SCK
		if (streamOutput == nil)
			return;

		ScreenStreamOutput* output = streamOutput;
		streamOutput = nil;
		_streamGeneration++;

		SCStream* stream = nil;
		{
			std::lock_guard<std::mutex> lock(output->_lock);
			output->_grabber = nullptr;
			stream = output->_stream;
			output->_stream = nil;
		}

		if (stream != nil)
		{
			// ownership of the stream and the output moves to the completion handler
			[stream stopCaptureWithCompletionHandler:^(NSError* error)
			{
				[stream release];
				[output release];
			}];
		}
		else
			[output release];

		Debug(_log, "Capture stream released");
	#endif
}

void macOsGrabber::streamFailed(quint64 generation, QString message)
{
	if (generation != _streamGeneration)
		return;

	Error(_log, "{:s}", message);

	uninit();
	QTimer::singleShot(3000, this, &macOsGrabber::start);
}

bool macOsGrabber::init_device(QString selectedDeviceName)
{	
	return true;
}

void macOsGrabber::decodeFrame(CGImageRef capturedImage)
{
	CFDataRef sysData = CGDataProviderCopyData(CGImageGetDataProvider(capturedImage));

	if (sysData != NULL)
	{
		uint8_t* rawData = (uint8_t*)CFDataGetBytePtr(sysData);
		_actualWidth = CGImageGetWidth(capturedImage);
		_actualHeight = CGImageGetHeight(capturedImage);

		size_t bytesPerRow = CGImageGetBytesPerRow(capturedImage);
		processSystemFrameBGRA(rawData, static_cast<int>(bytesPerRow));

		CFRelease(sysData);
	}
}

void macOsGrabber::grabFrame()
{	
	#ifdef MACOS_SCK
		// the display mode has changed: rebuild the stream for the new geometry
		if (!CGSizeEqualToSize(CGDisplayBounds(_actualDisplay).size, _streamPointSize))
		{
			Info(_log, "Display mode has changed. Rebuilding the capture stream");
			startStream();
			return;
		}

		CVPixelBufferRef frame = (streamOutput != nil) ? [streamOutput copyLatestFrame] : nullptr;

		if (frame == nullptr)
			return;

		if (CVPixelBufferLockBaseAddress(frame, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess)
		{
			_actualWidth = static_cast<int>(CVPixelBufferGetWidth(frame));
			_actualHeight = static_cast<int>(CVPixelBufferGetHeight(frame));

			// cropping is configured in native pixels but the stream is already scaled down
			if (_streamNativeWidth > 0)
			{
				_cropLeft = static_cast<int>(_nativeCropLeft * _actualWidth / _streamNativeWidth);
				_cropRight = static_cast<int>(_nativeCropRight * _actualWidth / _streamNativeWidth);
				_cropTop = static_cast<int>(_nativeCropTop * _actualWidth / _streamNativeWidth);
				_cropBottom = static_cast<int>(_nativeCropBottom * _actualWidth / _streamNativeWidth);
			}

			uint8_t* rawData = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(frame));
			size_t bytesPerRow = CVPixelBufferGetBytesPerRow(frame);

			if (rawData != nullptr)
				processSystemFrameBGRA(rawData, static_cast<int>(bytesPerRow));

			CVPixelBufferUnlockBaseAddress(frame, kCVPixelBufferLock_ReadOnly);
		}

		CVPixelBufferRelease(frame);
	#else
		bool stopNow = false;
		CGImageRef display;

		display = CGDisplayCreateImage(_actualDisplay);

		if (display == NULL)
		{			
			Error(_log, "Lost connection to the display or user didn't grant access rights");
			stopNow = true;
		}
		else
		{			
			decodeFrame(display);

			CGImageRelease(display);
		}

		if (stopNow)
		{
			uninit();
			QTimer::singleShot(3000, this, &macOsGrabber::start);
		}
	#endif	
}


void macOsGrabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;

	_nativeCropLeft = cropLeft;
	_nativeCropRight = cropRight;
	_nativeCropTop = cropTop;
	_nativeCropBottom = cropBottom;
}
