/* SoundCaptureMacOS.mm
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

#include <QString>
#include <QSemaphore>
#include <QTimer>
#include <cmath>
#include <stdio.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#include <utils/Logger.h>
#include <sound-capture/macos/SoundCaptureMacOS.h>

@class AudioStreamDelegate;

namespace {
	AudioStreamDelegate*	_avfSoundDelegate = nil;	
}

@interface AudioStreamDelegate : NSObject<AVCaptureAudioDataOutputSampleBufferDelegate>
{
@public
	SoundCaptureMacOS*	_avfGrabber;
	dispatch_queue_t	_sequencer;	
	AVCaptureSession*	_nativeSession;
}
- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
@end

@implementation AudioStreamDelegate

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
	if (CMSampleBufferGetNumSamples(sampleBuffer) < 1)
    {
		return;
    }
	if (!CMSampleBufferDataIsReady(sampleBuffer)) {        
        return;
    }
    if (_avfGrabber != nullptr)
    {
		CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
		size_t frameBytes;
		char* rawSoundData;
		if (CMBlockBufferGetDataPointer(blockBuffer, 0, NULL, &frameBytes, &rawSoundData) == kCMBlockBufferNoErr)
		{
			_avfGrabber->recordCallback(frameBytes, reinterpret_cast<uint8_t*>(rawSoundData));
		}    
    }
}

@end

SoundCaptureMacOS::SoundCaptureMacOS(const QJsonDocument& effectConfig, QObject* parent)
                            : SoundCapture(effectConfig, parent)
{		
	listDevices();
}


void SoundCaptureMacOS::listDevices()
{	
	AVCaptureDeviceDiscoverySession *session = [AVCaptureDeviceDiscoverySession 
			discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInMicrophone]
			mediaType:AVMediaTypeAudio
			position:AVCaptureDevicePositionUnspecified];

	if (session!=nullptr)
	{
		for (AVCaptureDevice* device in session.devices) 
		{	
			_availableDevices.append(QString(device.localizedName.UTF8String) + " (" + QString(device.uniqueID.UTF8String) + ")");
		}
	}
}

bool SoundCaptureMacOS::getPermission()
{
	
	if ([AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)])
	{       
        if ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio] == AVAuthorizationStatusAuthorized)
		{
            Info(_logger, "HyperHDR has the sound capture's permission");
			return true;	
        }
        else
		{            
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL grantedPerm) {
                if (grantedPerm) 
                    Info(_logger, "Got the sound capture permission. Please restart the application.");
                else
					Error(_logger, "HyperHDR has NOT been granted the sound capture's permission");                				
            } ];            
        }
    }
	else
		Error(_logger, "Selector for authorizationStatusForMediaType failed");

	Error(_logger, "HyperHDR have NOT got the sound capture's permission.");     
	return false;
}

SoundCaptureMacOS::~SoundCaptureMacOS()
{
	stopDevice();
}

void SoundCaptureMacOS::start()
{
	if (!getPermission())
		return;	
	
	if (_isActive && !_isRunning)
	{
		bool    		notFound = true;

		_soundBufferIndex = 0;
	
		for (AVCaptureDevice* device in [AVCaptureDeviceDiscoverySession 
										discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInMicrophone]
										mediaType:AVMediaTypeAudio
										position:AVCaptureDevicePositionUnspecified].devices) 
		{	
			QString current = QString(device.localizedName.UTF8String) + " (" + QString(device.uniqueID.UTF8String) + ")";
			if (current == _selectedDevice && notFound)
			{
				notFound = false;
				_avfSoundDelegate = [AudioStreamDelegate new];

				if (_avfSoundDelegate == nullptr)
				{
					Error(_logger,  "Audio listener creation failed");
				}
				else
				{							
					_avfSoundDelegate->_nativeSession = [AVCaptureSession new];

					NSError* apiError = nil;
					AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&apiError];

					if (!input) 
					{
						Error(_logger,  "Could not open capturing sound device: %s", apiError.localizedDescription.UTF8String);        
					}
					else
					{
						Info(_logger,  "Opening: %s", QSTRING_CSTR(current));

						[_avfSoundDelegate->_nativeSession addInput:input];

						[device lockForConfiguration:NULL];

						AVCaptureAudioDataOutput* output = [AVCaptureAudioDataOutput new];

						[_avfSoundDelegate->_nativeSession addOutput:output];
						output.audioSettings = nil;

						AudioChannelLayout channelLayout;
						bzero( &channelLayout, sizeof(channelLayout));
						channelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
						
						output.audioSettings = [NSDictionary dictionaryWithObjectsAndKeys:
												[ NSNumber numberWithInt: kAudioFormatLinearPCM ], AVFormatIDKey,
												[ NSNumber numberWithInt: 1 ], AVNumberOfChannelsKey,
												[ NSNumber numberWithFloat: 22050.0 ], AVSampleRateKey,
												[ NSNumber numberWithBool: NO], AVLinearPCMIsFloatKey,
												[ NSNumber numberWithBool: NO], AVLinearPCMIsNonInterleaved,
												[ NSNumber numberWithInt: 16], AVLinearPCMBitDepthKey,
												[ NSNumber numberWithBool: NO], AVLinearPCMIsBigEndianKey,
												[ NSData dataWithBytes: &channelLayout length: sizeof(channelLayout) ], AVChannelLayoutKey,
												nil];																					
		
						_avfSoundDelegate->_avfGrabber = this;
						_avfSoundDelegate->_sequencer = dispatch_queue_create("AudioDataOutputQueue", DISPATCH_QUEUE_SERIAL);
						[output setSampleBufferDelegate:_avfSoundDelegate queue:_avfSoundDelegate->_sequencer];

						if (!(_avfSoundDelegate->_nativeSession.running))
						{
							[_avfSoundDelegate->_nativeSession startRunning];
							if ((_avfSoundDelegate->_nativeSession.running))
								Info(_logger,  "AVF audio grabber starts capturing");
							else
								Error(_logger,  "AVF audio grabber isn't capturing");
						}			
		
						[device unlockForConfiguration];

						Info(_logger,  "AVF audio grabber initialized successfully");
						_isRunning = true;						
					}
				}				
			}
		}

		if (notFound)
		{
			Error(_logger, "Could not find selected audio device: '%s' in the system.", QSTRING_CSTR(_selectedDevice));
			return;
		}
	}
}

void SoundCaptureMacOS::stop()
{
	stopDevice();
}

void SoundCaptureMacOS::recordCallback(uint32_t frameBytes, uint8_t* rawAudioData)
{
	int wantedSize = static_cast<int>(sizeof(_soundBuffer));

	while (frameBytes > 0)
	{
		int sourceCap = wantedSize - _soundBufferIndex;
		int incomingCap = frameBytes;
		int toRead = std::min(sourceCap, incomingCap);
		uint8_t* destStart = reinterpret_cast<uint8_t*>(&_soundBuffer) + _soundBufferIndex;

		if (toRead <= 0)
			break;

		memcpy(destStart, rawAudioData, toRead);

		rawAudioData += toRead;
		frameBytes -= toRead;
		_soundBufferIndex = (_soundBufferIndex + toRead) % wantedSize;
		if (_soundBufferIndex == 0)
			analyzeSpectrum(_soundBuffer, SOUNDCAPMACOS_BUF_LENP);
	}
}

void SoundCaptureMacOS::stopDevice()
{
	if (!_isRunning)
		return;

	Info(_logger,  "Disconnecting from sound driver: '%s'", QSTRING_CSTR(_selectedDevice));

		
	_isRunning = false;

	if (_avfSoundDelegate!=nullptr && _avfSoundDelegate->_nativeSession  != nullptr)
	{
		[_avfSoundDelegate->_nativeSession stopRunning];
		[_avfSoundDelegate->_nativeSession release];
		_avfSoundDelegate->_nativeSession = nullptr;
		_avfSoundDelegate->_sequencer = nullptr;				
	}
	_avfSoundDelegate = nullptr;
	Info(_logger,  "AVF sound grabber uninit");
};
