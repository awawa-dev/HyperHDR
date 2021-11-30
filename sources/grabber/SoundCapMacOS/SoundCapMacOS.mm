/* SoundCapMacOS.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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

#include <grabber/SoundCapMacOS.h>
#include <cmath>
#include <QString>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

size_t     SoundCapMacOS::_soundBufferIndex = 0;
int16_t    SoundCapMacOS::_soundBuffer[(1<<SOUNDCAPMACOS_BUF_LENP)*2];



@interface AudioStreamDelegate : NSObject<AVCaptureAudioDataOutputSampleBufferDelegate>
{
@public
	SoundCapMacOS*		_avfGrabber;
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
			_avfGrabber->RecordCallback(frameBytes, (uint8_t*) rawSoundData);
		}    
    }
}

@end

AudioStreamDelegate* _avfSoundDelegate = nullptr;

SoundCapMacOS::SoundCapMacOS(const QJsonDocument& effectConfig, QObject* parent)
                            : SoundCapture(effectConfig, parent)
{		
	ListDevices();
}


void SoundCapMacOS::ListDevices()
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

bool SoundCapMacOS::getPermission()
{
	
	if ([AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)])
	{       
        if ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio] == AVAuthorizationStatusAuthorized)
		{
            Info(Logger::getInstance("HYPERHDR"), "HyperHDR has the sound capture's permission");
			return true;	
        }
        else
		{            
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL grantedPerm) {
                if (grantedPerm) 
                    Info(Logger::getInstance("HYPERHDR"), "Got the sound capture permission. Please restart the application.");
                else
					Error(Logger::getInstance("HYPERHDR"), "HyperHDR has NOT been granted the sound capture's permission");                				
            } ];            
        }
    }
	else
		Error(Logger::getInstance("HYPERHDR"), "Selector for authorizationStatusForMediaType failed");

	Error(Logger::getInstance("HYPERHDR"), "HyperHDR have NOT got the sound capture's permission.");     
	return false;
}

SoundCapMacOS::~SoundCapMacOS()
{
	Stop();
}


void SoundCapMacOS::RecordCallback(uint32_t frameBytes, uint8_t* rawAudioData)
{
	try
	{		
		if (_isRunning)
		{
			size_t     destSize = (1 << SOUNDCAPMACOS_BUF_LENP)*sizeof(int16_t);
			uint8_t*   destStart = (uint8_t*)(&_soundBuffer) + _soundBufferIndex;
			uint8_t*   destEnd   = (uint8_t*)(&_soundBuffer) + destSize;

			for(;destStart < destEnd && _soundBufferIndex < destSize && frameBytes > 0; destStart++, _soundBufferIndex++, rawAudioData++, frameBytes--)
			{
				*destStart = *rawAudioData;
			}
			
			if (_soundBufferIndex == destSize && AnaliseSpectrum(_soundBuffer, SOUNDCAPMACOS_BUF_LENP))
			{
				_resultIndex++;
				_soundBufferIndex = 0;
				destStart = (uint8_t*)(&_soundBuffer);
			}

			for(;destStart < destEnd && _soundBufferIndex < destSize && frameBytes > 0; destStart++, _soundBufferIndex++, rawAudioData++, frameBytes--)
			{
				*destStart = *rawAudioData;
			}
		}
	}
	catch(...)
	{
	}
}

void SoundCapMacOS::Start()
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
					Error(Logger::getInstance("HYPERHDR"),  "Audio listener creation failed");
				}
				else
				{							
					_avfSoundDelegate->_nativeSession = [AVCaptureSession new];

					NSError* apiError = nil;
					AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&apiError];

					if (!input) 
					{
						Error(Logger::getInstance("HYPERHDR"),  "Could not open capturing sound device: %s", apiError.localizedDescription.UTF8String);        
					}
					else
					{
						Info(Logger::getInstance("HYPERHDR"),  "Opening: %s", QSTRING_CSTR(current));

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
								Info(Logger::getInstance("HYPERHDR"),  "AVF audio grabber starts capturing");
							else
								Error(Logger::getInstance("HYPERHDR"),  "AVF audio grabber isn't capturing");
						}			
		
						[device unlockForConfiguration];

						Info(Logger::getInstance("HYPERHDR"),  "AVF audio grabber initialized successfully");
						_isRunning = true;						
					}
				}				
			}
		}

		if (notFound)
		{
			Error(Logger::getInstance("HYPERHDR"), "Could not find selected audio device: '%s' in the system.", QSTRING_CSTR(_selectedDevice));
			return;
		}
	}
}

void SoundCapMacOS::Stop()
{
	
	if (!_isRunning)
		return;

	Info(Logger::getInstance("HYPERHDR"),  "Disconnecting from sound driver: '%s'", QSTRING_CSTR(_selectedDevice));

		
	_isRunning = false;

	if (_avfSoundDelegate!=nullptr && _avfSoundDelegate->_nativeSession  != nullptr)
	{
		[_avfSoundDelegate->_nativeSession stopRunning];
		[_avfSoundDelegate->_nativeSession release];
		_avfSoundDelegate->_nativeSession = nullptr;
		_avfSoundDelegate->_sequencer = nullptr;				
	}
	_avfSoundDelegate = nullptr;
	Info(Logger::getInstance("HYPERHDR"),  "AVF sound grabber uninit");
}
