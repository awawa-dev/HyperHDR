/* AVFGrabber.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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
#include <stdio.h>

#include <base/HyperHdrInstance.h>
#include <utils/GlobalSignals.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include <grabber/osx/AVF/AVFGrabber.h>

// Apple frameworks
#include <Accelerate/Accelerate.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>



@interface VideoStreamDelegate : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
@public
	AVFGrabber*			_avfGrabber;
	dispatch_queue_t	_sequencer;	
	AVCaptureSession*	_nativeSession;
}
- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
- (void)captureOutput:(AVCaptureOutput *)output didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
@end

@implementation VideoStreamDelegate

- (void)captureOutput:(AVCaptureOutput *)output didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
}

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
	if (CMSampleBufferGetNumSamples(sampleBuffer) < 1)
	{
		return;
	}

	if (_avfGrabber != nullptr)
	{
		CVPixelBufferRef systemBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
		if (CVPixelBufferLockBaseAddress(systemBuffer, 0) == kCVReturnSuccess)
		{
			const uint8_t* rawVideoData = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(systemBuffer));
			uint32_t frameBytes = CVPixelBufferGetHeight(systemBuffer) * CVPixelBufferGetBytesPerRow(systemBuffer);

			_avfGrabber->receive_image(rawVideoData, frameBytes, QString(""));

			CVPixelBufferUnlockBaseAddress(systemBuffer, 0);
		}
	}
}

@end

VideoStreamDelegate* _avfDelegate = nullptr;

AVFGrabber::format_t fmt_array[] =
{
	{ "YUVS",  PixelFormat::YUYV },
	{ "420V",  PixelFormat::NV12 },
	{ "DMB1",  PixelFormat::MJPEG }
};

AVFGrabber::AVFGrabber(const QString& device, const QString& configurationPath)
	: Grabber(configurationPath, "macOS AVF:" + device.left(14))
	, _permission(false)
{
	// init AVF
	_isAVF = false;

	if (!getPermission())
	{
		Warning(_log, "HyperHDR has NOT been granted the camera's permission. Will check it later again.");
		QTimer::singleShot(3000, this, &AVFGrabber::getPermission);
	}

	_isAVF = true;

	// Refresh devices
	enumerateAVFdevices();
}

bool AVFGrabber::getPermission()
{
	if (_permission)
		return true;

	if ([AVCaptureDevice respondsToSelector : @selector(authorizationStatusForMediaType:)])
	{
		if ([AVCaptureDevice authorizationStatusForMediaType : AVMediaTypeVideo] == AVAuthorizationStatusAuthorized)
		{
			Info(_log, "HyperHDR has the camera's permission");
			_permission = true;
		}
		else if (_isAVF)
		{
			[AVCaptureDevice requestAccessForMediaType : AVMediaTypeVideo completionHandler : ^ (BOOL grantedPerm) {

				_permission = grantedPerm;

				if (_permission)
					Info(_log, "HyperHDR has been granted the camera's permission");
				else
					Error(_log, "HyperHDR has NOT been granted the camera's permission");

				if (_isAVF)
				{
					if (!_permission)
						QMetaObject::invokeMethod(this, [this]{ QTimer::singleShot(5000, this, &AVFGrabber::getPermission); });
					else
					{
						Info(_log, "Got the video permission. Now trying to start HyperHDR's video grabber.");
						start();
					}
				}
			} ];
		}
	}
	else
		Error(_log, "Selector for authorizationStatusForMediaType failed");

	return _permission;
}

QString AVFGrabber::GetSharedLut()
{
	QString ret = QString("%1%2").arg(QCoreApplication::applicationDirPath()).arg("/../lut");
	QFileInfo info(ret);
	ret = info.absoluteFilePath();
	return ret;
}

void AVFGrabber::loadLutFile(PixelFormat color)
{
	// load lut table
	QString fileName1 = QString("%1%2").arg(_configurationPath).arg("/lut_lin_tables.3d");
	QString fileName2 = QString("%1%2").arg(GetSharedLut()).arg("/lut_lin_tables.3d");

	Grabber::loadLutFile(_log, color, QList<QString>{fileName1, fileName2});
}

void AVFGrabber::setHdrToneMappingEnabled(int mode)
{
	if (_hdrToneMappingEnabled != mode || _lut.data() == nullptr)
	{
		_hdrToneMappingEnabled = mode;
		if (_lut.data() != nullptr || !mode)
			Debug(_log, "setHdrToneMappingMode to: %s", (mode == 0) ? "Disabled" : ((mode == 1) ? "Fullscreen" : "Border mode"));
		else
			Warning(_log, "setHdrToneMappingMode to: enable, but the LUT file is currently unloaded");

		if (_AVFWorkerManager.isActive())
		{
			Debug(_log, "setHdrToneMappingMode replacing LUT and restarting");
			_AVFWorkerManager.Stop();
			if ((_actualVideoFormat == PixelFormat::YUYV) || (_actualVideoFormat == PixelFormat::I420) || (_actualVideoFormat == PixelFormat::NV12))
				loadLutFile(PixelFormat::YUYV);
			else
				loadLutFile(PixelFormat::RGB24);
			_AVFWorkerManager.Start();
		}
		emit SignalSetNewComponentStateToAllInstances(hyperhdr::Components::COMP_HDR, (mode != 0));
	}
	else
		Debug(_log, "setHdrToneMappingMode nothing changed: %s", (mode == 0) ? "Disabled" : ((mode == 1) ? "Fullscreen" : "Border mode"));
}

AVFGrabber::~AVFGrabber()
{
	uninit();
}

void AVFGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
		stop();
	}
}

bool AVFGrabber::init()
{
	if (!_permission)
	{
		Warning(_log, "HyperHDR has NOT been granted the camera's permission");
		return false;
	}

	if (!_initialized && _isAVF)
	{
		QString foundDevice = "";
		int     foundIndex = -1;
		int     bestGuess = -1;
		int     bestGuessMinX = INT_MAX;
		int     bestGuessMinFPS = INT_MAX;
		bool    autoDiscovery = (QString::compare(_deviceName, Grabber::AUTO_SETTING, Qt::CaseInsensitive) == 0);

		enumerateAVFdevices(true);

		if (!autoDiscovery && !_deviceProperties.contains(_deviceName))
		{
			Debug(_log, "Device %s is not available. Changing to auto.", QSTRING_CSTR(_deviceName));
			autoDiscovery = true;
		}

		if (autoDiscovery)
		{
			Debug(_log, "Forcing auto discovery device");
			if (_deviceProperties.count() > 0)
			{
				foundDevice = _deviceProperties.firstKey();
				_deviceName = foundDevice;
				Debug(_log, "Auto discovery set to %s", QSTRING_CSTR(_deviceName));
			}
		}
		else
			foundDevice = _deviceName;

		if (foundDevice.isNull() || foundDevice.isEmpty() || !_deviceProperties.contains(foundDevice))
		{
			Error(_log, "Could not find any capture device");
			return false;
		}



		DeviceProperties dev = _deviceProperties[foundDevice];

		Debug(_log, "Searching for %s %d x %d @ %d fps (%s)", QSTRING_CSTR(foundDevice), _width, _height, _fps, QSTRING_CSTR(pixelFormatToString(_enc)));



		for (int i = 0; i < dev.valid.count() && foundIndex < 0; ++i)
		{
			bool strict = false;
			const auto& val = dev.valid[i];

			if (bestGuess == -1 || (val.x <= bestGuessMinX && val.x >= 640 && val.fps <= bestGuessMinFPS && val.fps >= 10))
			{
				bestGuess = i;
				bestGuessMinFPS = val.fps;
				bestGuessMinX = val.x;
			}

			if (_width && _height)
			{
				strict = true;
				if (val.x != _width || val.y != _height)
					continue;
			}

			if (_fps && _fps != Grabber::AUTO_FPS)
			{
				strict = true;
				if (val.fps != _fps)
					continue;
			}

			if (_enc != PixelFormat::NO_CHANGE)
			{
				strict = true;
				if (val.pf != _enc)
					continue;
			}

			if (strict && (val.fps <= 60 || _fps != Grabber::AUTO_FPS))
			{
				foundIndex = i;
			}
		}

		if (foundIndex < 0 && bestGuess >= 0)
		{
			if (!autoDiscovery)
				Warning(_log, "Selected resolution not found in supported modes. Forcing best guess");
			else
				Warning(_log, "Forcing best guess");

			foundIndex = bestGuess;
		}

		if (foundIndex >= 0)
		{
			Info(_log, "*************************************************************************************************");
			Info(_log, "Starting AVF grabber. Selected: '%s' %d x %d @ %d fps %s", QSTRING_CSTR(foundDevice),
				dev.valid[foundIndex].x, dev.valid[foundIndex].y, dev.valid[foundIndex].fps,
				QSTRING_CSTR(pixelFormatToString(dev.valid[foundIndex].pf)));
			Info(_log, "*************************************************************************************************");

			if (init_device(foundDevice, dev.valid[foundIndex]))
				_initialized = true;
		}
		else
			Error(_log, "Could not find any capture device settings");
	}

	return _initialized;
}


PixelFormat AVFGrabber::identifyFormat(QString format)
{
	for (int i = 0; i < sizeof(fmt_array) / sizeof(fmt_array[0]); i++)
		if (format == fmt_array[i].formatName)
		{
			return fmt_array[i].pixel;
		}

	return PixelFormat::NO_CHANGE;
}


QString AVFGrabber::FormatRes(int w, int h, QString format)
{
	QString ws = QString::number(w);
	QString hs = QString::number(h);

	while (ws.length() < 4)
		ws = " " + ws;
	while (hs.length() < 4)
		hs = " " + hs;

	return ws + "x" + hs + " " + format;
}

QString AVFGrabber::FormatFrame(int fr)
{
	QString frame = QString::number(fr);

	while (frame.length() < 2)
		frame = " " + frame;

	return frame;
}

void AVFGrabber::enumerateAVFdevices()
{
	enumerateAVFdevices(false);
}

QString AVFGrabber::getIdOfCodec(uint32_t cc4)
{
	QString result = "";
	for (uint32_t i = 0; i < 4; i++)
	{
		result = static_cast<char>(cc4 & 0xFF) + result;
		cc4 >>= 8;
	}
	return result.toUpper();
}

void AVFGrabber::enumerateAVFdevices(bool silent)
{
	if (!_isAVF)
	{
		Error(_log, "AVF is not init. Exiting.");
		return;
	}

	Debug(_log, "Searching for devices...");


	_deviceProperties.clear();

	AVCaptureDeviceDiscoverySession* session = [AVCaptureDeviceDiscoverySession
		discoverySessionWithDeviceTypes : @[AVCaptureDeviceTypeExternalUnknown]
		mediaType:AVMediaTypeVideo
		position : AVCaptureDevicePositionUnspecified];

	if (session != nullptr)
	{
		for (AVCaptureDevice* device in session.devices)
		{
			DeviceProperties properties;
			properties.name = QString(device.localizedName.UTF8String) + " (" + QString(device.uniqueID.UTF8String) + ")";

			for (AVCaptureDeviceFormat* format in device.formats)
			{
				for (AVFrameRateRange* frameRateRange in format.videoSupportedFrameRateRanges) {
					for (int frameRate = frameRateRange.minFrameRate; frameRate <= frameRateRange.maxFrameRate; frameRate++) {
						CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
						uint32_t cc4 = CMFormatDescriptionGetMediaSubType(format.formatDescription);

						QString sFormat = getIdOfCodec(cc4);
						QString sFrame = FormatFrame(frameRate);
						QString resolution = FormatRes(dims.width, dims.height, sFormat);
						QString displayResolutions = FormatRes(dims.width, dims.height, "");

						if (!properties.displayResolutions.contains(displayResolutions))
							properties.displayResolutions << displayResolutions;

						if (!properties.framerates.contains(sFrame))
							properties.framerates << sFrame;

						DevicePropertiesItem di;
						di.x = dims.width;
						di.y = dims.height;
						di.fps = frameRate;
						di.pf = identifyFormat(sFormat);
						di.fourcc = cc4;

						if (di.pf != PixelFormat::NO_CHANGE)
							properties.valid.append(di);

						if (!silent)
						{
							if (di.pf != PixelFormat::NO_CHANGE)
								Debug(_log, "%s %d x %d @ %d fps %s (%s)", QSTRING_CSTR(properties.name), di.x, di.y, di.fps, QSTRING_CSTR(sFormat), QSTRING_CSTR(pixelFormatToString(di.pf)));
							else
								Debug(_log, "%s %d x %d @ %d fps %s (unsupported)", QSTRING_CSTR(properties.name), di.x, di.y, di.fps, QSTRING_CSTR(sFormat));
						}
					}
				}
			}
			_deviceProperties.insert(properties.name, properties);
		}
	}
}

bool AVFGrabber::start()
{
	try
	{
		resetCounter(InternalClock::now());
		_AVFWorkerManager.Start();

		if (_AVFWorkerManager.workersCount <= 1)
			Info(_log, "Multithreading for AVF is disabled. Available thread's count %d", _AVFWorkerManager.workersCount);
		else
			Info(_log, "Multithreading for AVF is enabled. Available thread's count %d", _AVFWorkerManager.workersCount);

		if (init())
		{
			start_capturing();
			Info(_log, "Started");
			return true;
		}
	}
	catch (std::exception& e)
	{
		Error(_log, "Start failed (%s)", e.what());
	}

	return false;
}

void AVFGrabber::stop()
{
	if (_initialized)
	{
		_AVFWorkerManager.Stop();
		uninit_device();
		_initialized = false;
		Info(_log, "Stopped");
	}
}

bool AVFGrabber::init_device(QString selectedDeviceName, DevicePropertiesItem props)
{
	bool result = false;
	bool notFound = true;
	PixelFormat pixelformat = props.pf;;
	QString sFormat = pixelFormatToString(pixelformat), error;
	QString guid = _deviceProperties[selectedDeviceName].name;
	CMTime minFrameDuration;

	Debug(_log, "Init_device: %s, %d x %d @ %d fps (%s) => %s", QSTRING_CSTR(selectedDeviceName), props.x, props.y, props.fps, QSTRING_CSTR(sFormat), QSTRING_CSTR(guid));


	for (AVCaptureDevice* device in[AVCaptureDeviceDiscoverySession
		discoverySessionWithDeviceTypes : @[AVCaptureDeviceTypeExternalUnknown]
		mediaType:AVMediaTypeVideo
		position : AVCaptureDevicePositionUnspecified].devices)
	{
		QString devName = QString(device.localizedName.UTF8String) + " (" + QString(device.uniqueID.UTF8String) + ")";

		if (devName == selectedDeviceName && notFound)
		{
			for (AVCaptureDeviceFormat* format in device.formats)
				if (notFound)
				{
					uint32_t cc4 = CMFormatDescriptionGetMediaSubType(format.formatDescription);
					if (props.fourcc != cc4)
						continue;

					for (AVFrameRateRange* frameRateRange in format.videoSupportedFrameRateRanges)
						if (notFound)
						{
							CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
							int minF = frameRateRange.minFrameRate, maxF = frameRateRange.maxFrameRate;
							uint32_t cc4 = CMFormatDescriptionGetMediaSubType(format.formatDescription);
							QString sFormat = getIdOfCodec(cc4);
							if (props.x == dims.width &&
								props.y == dims.height &&
								props.fps >= minF && props.fps <= maxF &&
								props.fourcc == cc4)
							{
								minFrameDuration = frameRateRange.minFrameDuration;
								notFound = false;
							}
						}

					if (!notFound)
					{
						_avfDelegate = [VideoStreamDelegate new];

						if (_avfDelegate == nullptr)
						{
							Error(_log, "Listener creation failed");
						}
						else
						{
							_avfDelegate->_nativeSession = [AVCaptureSession new];

							NSError* apiError = nil;
							AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice : device error : &apiError];

							if (!input)
							{
								Error(_log, "Could not open video capturing device: %s", apiError.localizedDescription.UTF8String);
							}
							else
							{
								Info(_log, "Opening: %s", QSTRING_CSTR(selectedDeviceName));

								[_avfDelegate->_nativeSession addInput : input] ;

								[device lockForConfiguration : NULL] ;
								device.activeFormat = format;

								@try {
									[device setActiveVideoMinFrameDuration : minFrameDuration] ;
								}
								@catch (NSException* exception) {
									Warning(_log, "Device doesn't support setting min framerate");
								}
								@try {
									[device setActiveVideoMaxFrameDuration : minFrameDuration] ;
								}
								@catch (NSException* exception) {
									Warning(_log, "Device doesn't support setting max framerate");
								}

								AVCaptureVideoDataOutput* output = [AVCaptureVideoDataOutput new];

								[_avfDelegate->_nativeSession addOutput : output] ;
								output.videoSettings = nil;

								switch (pixelformat)
								{
									case PixelFormat::YUYV:
									{
										output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys :
										[NSNumber numberWithUnsignedInt : kCMPixelFormat_422YpCbCr8_yuvs] , (id)kCVPixelBufferPixelFormatTypeKey,
											nil];
									}
									break;
									case PixelFormat::NV12:
									{
										output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys :
										[NSNumber numberWithUnsignedInt : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange] , (id)kCVPixelBufferPixelFormatTypeKey,
											nil];
									}
									break;
									case PixelFormat::MJPEG:
									{
										output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys :
										[NSNumber numberWithUnsignedInt : kCMPixelFormat_422YpCbCr8_yuvs] , (id)kCVPixelBufferPixelFormatTypeKey,
											nil];
										pixelformat = PixelFormat::YUYV;
										Warning(_log, "Let macOS do accelerated MJPEG decoding to YUYV");
									}
									break;
									default:
									{
										Error(_log, "Unsupported encoding");
									}
								}

								if (output.videoSettings != nil)
								{
									_actualVideoFormat = pixelformat;
									_actualWidth = props.x;
									_actualHeight = props.y;
									_actualFPS = props.fps;
									_actualDeviceName = selectedDeviceName;

									switch (_actualVideoFormat)
									{

										case PixelFormat::YUYV:
										{
											loadLutFile(PixelFormat::YUYV);
											_frameByteSize = props.x * props.y * 2;
											_lineLength = props.x * 2;
										}
										break;

										case PixelFormat::NV12:
										{
											loadLutFile(PixelFormat::YUYV);
											_frameByteSize = (6 * props.x * props.y) / 4;
											_lineLength = props.x;
										}
										break;

										default:
										{
											Error(_log, "Unsupported encoding");
											output.videoSettings = nil;
										}
									}
								}

								if (output.videoSettings != nil)
								{
									[output setAlwaysDiscardsLateVideoFrames : true] ;

									_avfDelegate->_avfGrabber = this;
									_avfDelegate->_sequencer = dispatch_queue_create("VideoDataOutputQueue", DISPATCH_QUEUE_SERIAL);
									[output setSampleBufferDelegate : _avfDelegate queue : _avfDelegate->_sequencer] ;

									if (!(_avfDelegate->_nativeSession.running))
									{
										[_avfDelegate->_nativeSession startRunning] ;
										Info(_log, "AVF video grabber starts capturing");
									}

									[device unlockForConfiguration];

									Info(_log, "AVF video grabber initialized successfully");
									result = true;
								}
							}
						}
					}
				}
		}
	}

	if (notFound)
	{
		Warning(_log, "Could not found video grabber configuration");
	}

	if (!result)
	{
		uninit_device();
	}

	return result;
}

void AVFGrabber::uninit_device()
{
	if (_avfDelegate != nullptr && _avfDelegate->_nativeSession != nullptr)
	{
		[_avfDelegate->_nativeSession stopRunning] ;
		[_avfDelegate->_nativeSession release] ;
		_avfDelegate->_nativeSession = nullptr;
		_avfDelegate->_sequencer = nullptr;
	}
	_avfDelegate = nullptr;
	Info(_log, "AVF video grabber uninit");
}

void AVFGrabber::start_capturing()
{
	if (_avfDelegate != nullptr && _avfDelegate->_nativeSession != nullptr)
	{
		if (_avfDelegate->_nativeSession.running)
		{
			Info(_log, "AVF video grabber already capturing");
		}
		else
			Warning(_log, "AVF video grabber cant start capturing");
	}
}

void AVFGrabber::receive_image(const void* frameImageBuffer, int size, QString message)
{
	if (frameImageBuffer == NULL || size == 0)
		Error(_log, "Received empty image frame: %s", QSTRING_CSTR(message));
	else
	{
		if (!message.isEmpty())
			Debug(_log, "Received image frame: %s", QSTRING_CSTR(message));

		process_image(frameImageBuffer, size);
	}
}

bool AVFGrabber::process_image(const void* frameImageBuffer, int size)
{
	bool		frameSend = false;
	uint64_t	processFrameIndex = _currentFrame++;

	// frame skipping
	if ((processFrameIndex % _fpsSoftwareDecimation != 0) && (_fpsSoftwareDecimation > 1))
		return frameSend;

	// We do want a new frame...
	if (size < _frameByteSize && _actualVideoFormat != PixelFormat::MJPEG)
	{
		Error(_log, "Frame too small: %d != %d", size, _frameByteSize);
	}
	else
	{
		if (_AVFWorkerManager.isActive())
		{
			// stats
			int64_t now = InternalClock::now();
			int64_t diff = now - frameStat.frameBegin;
			int64_t prevToken = frameStat.token;

			if (frameStat.token <= 0 || diff < 0)
			{
				frameStat.token = PerformanceCounters::currentToken();
				resetCounter(now);
			}
			else if (prevToken != (frameStat.token = PerformanceCounters::currentToken()))
			{
				int total = (frameStat.badFrame + frameStat.goodFrame);
				int av = (frameStat.goodFrame > 0) ? frameStat.averageFrame / frameStat.goodFrame : 0;
				QString access = (frameStat.directAccess) ? " (direct)" : "";
				if (diff >= 59000 && diff <= 65000)
					emit GlobalSignals::getInstance()->SignalPerformanceNewReport(
					PerformanceReport(hyperhdr::PerformanceReportType::VIDEO_GRABBER, frameStat.token, this->_actualDeviceName + access, total / qMax(diff / 1000.0, 1.0), av, frameStat.goodFrame, frameStat.badFrame));
				
				resetCounter(now);

				QString currentCache = QString::fromStdString(Image<ColorRgb>::adjustCache());

				if (!currentCache.isEmpty())
					Info(_log, "%s", QSTRING_CSTR(currentCache));
			}

			if (_AVFWorkerManager.workers == nullptr)
			{
				_AVFWorkerManager.InitWorkers();
				Debug(_log, "Worker's thread count  = %d", _AVFWorkerManager.workersCount);

				for (unsigned int i = 0; i < _AVFWorkerManager.workersCount && _AVFWorkerManager.workers != nullptr; i++)
				{
					AVFWorker* _workerThread = _AVFWorkerManager.workers[i];
					connect(_workerThread, &AVFWorker::SignalNewFrameError, this, &AVFGrabber::newWorkerFrameErrorHandler);
					connect(_workerThread, &AVFWorker::SignalNewFrame, this, &AVFGrabber::newWorkerFrameHandler);
				}
			}

			for (unsigned int i = 0; _AVFWorkerManager.isActive() && i < _AVFWorkerManager.workersCount && _AVFWorkerManager.workers != nullptr; i++)
			{
				if (_AVFWorkerManager.workers[i]->isFinished() || !_AVFWorkerManager.workers[i]->isRunning())
				{
					if (_AVFWorkerManager.workers[i]->isBusy() == false)
					{
						AVFWorker* _workerThread = _AVFWorkerManager.workers[i];

						if ((_actualVideoFormat == PixelFormat::YUYV || _actualVideoFormat == PixelFormat::I420 ||
							_actualVideoFormat == PixelFormat::NV12) && !_lutBufferInit)
						{
							loadLutFile();
						}

						bool directAccess = !(_signalAutoDetectionEnabled || _signalDetectionEnabled || isCalibrating());
						_workerThread->setup(
							i,
							_actualVideoFormat,
							(uint8_t*)frameImageBuffer, size, _actualWidth, _actualHeight, _lineLength,
							_cropLeft, _cropTop, _cropBottom, _cropRight,
							processFrameIndex, InternalClock::nowPrecise(), _hdrToneMappingEnabled,
							(_lutBufferInit) ? _lut.data() : nullptr, _qframe, directAccess, _deviceName);

						if (_AVFWorkerManager.workersCount > 1)
							_AVFWorkerManager.workers[i]->start();
						else
							_AVFWorkerManager.workers[i]->startOnThisThread();

						frameSend = true;
						break;
					}
				}
			}
		}
	}

	return frameSend;
}

void AVFGrabber::newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount)
{

	frameStat.badFrame++;
	//Debug(_log, "Error occured while decoding mjpeg frame %d = %s", sourceCount, QSTRING_CSTR(error));	

	// get next frame	
	if (workerIndex > _AVFWorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);

	if (workerIndex <= _AVFWorkerManager.workersCount)
		_AVFWorkerManager.workers[workerIndex]->noBusy();

}


void AVFGrabber::newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin)
{
	handleNewFrame(workerIndex, image, sourceCount, _frameBegin);

	// get next frame	
	if (workerIndex > _AVFWorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);

	if (workerIndex <= _AVFWorkerManager.workersCount)
		_AVFWorkerManager.workers[workerIndex]->noBusy();
}
