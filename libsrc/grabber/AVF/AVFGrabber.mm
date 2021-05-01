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

#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/HyperHdrIManager.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include "grabber/AVFGrabber.h"
#include "utils/ColorSys.h"

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
            const uint8_t *rawVideoData = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(systemBuffer));
            uint32_t frameBytes = CVPixelBufferGetHeight(systemBuffer) * CVPixelBufferGetBytesPerRow(systemBuffer);
			
            _avfGrabber->receive_image(rawVideoData, frameBytes, QString(""));

            CVPixelBufferUnlockBaseAddress(systemBuffer, 0);
        }
    }
}

@end

VideoStreamDelegate* _avfDelegate = nullptr;

struct AVFGrabber::DeviceProperties
{
	QString					name		= QString();
	QMultiMap<QString, int>	inputs		= QMultiMap<QString, int>();
	QStringList				displayResolutions = QStringList();		
	QStringList				framerates	= QStringList();	
	QList<DevicePropertiesItem>    valid = QList<DevicePropertiesItem>();
};

struct AVFGrabber::DevicePropertiesItem
{
	uint32_t	fourcc;
	int 		x,y,fps,fps_a,fps_b;
	PixelFormat pf;		
};


#define CLEAR(x) memset(&(x), 0, sizeof(x))

// some stuff for HDR tone mapping
#define LUT_FILE_SIZE 50331648

AVFGrabber::format_t fmt_array[] =
{
	{ "YUVS",  PixelFormat::YUYV },
	{ "420V",  PixelFormat::NV12 },
	{ "DMB1",  PixelFormat::MJPEG }
};

AVFGrabber::AVFGrabber(const QString & device
		, unsigned width
		, unsigned height
		, unsigned fps
		, unsigned input		
		, PixelFormat pixelFormat
		, const QString & configurationPath
		)
	: Grabber("V4L2:macOS AVF:"+device)
	, _deviceName()	
	, _fileDescriptor(-1)
	, _pixelFormat(pixelFormat)
	, _lineLength(-1)
	, _frameByteSize(-1)
	, _noSignalCounterThreshold(40)
	, _noSignalThresholdColor(ColorRgb{0,0,0})
	, _signalDetectionEnabled(true)	
	, _noSignalDetected(false)
	, _noSignalCounter(0)
	, _x_frac_min(0.25)
	, _y_frac_min(0.25)
	, _x_frac_max(0.75)
	, _y_frac_max(0.75)
	, _initialized(false)
	, _deviceAutoDiscoverEnabled(false)	
	, _fpsSoftwareDecimation(1)
	, lutBuffer(NULL)
	, _lutBufferInit(false)
	, _currentFrame(0)
	, _configurationPath(configurationPath)
	, _enc(pixelFormat)
	, _brightness(0)
	, _contrast(0)
	, _qframe(false)
	, _permission(false)
	
{	
	// init
	_isAVF = false;

	if (!getPermission())
	{
		Warning(_log, "HyperHDR has NOT been granted the camera's permission. Will check it later again.");
		QTimer::singleShot(3000, this, SLOT(getPermission()));
	}
	
	_isAVF = true;	
		
	// devices
	getV4Ldevices();

	// init
	setInput(input);
	setWidthHeight(width, height);
	setFramerate(fps);
	setDeviceVideoStandard(device);
	Debug(_log,"Init pixel format: %i", static_cast<int>(_pixelFormat));	
}

bool AVFGrabber::getPermission()
{
	if (_permission)
		return true;

	if ([AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)])
	{       
        if ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo] == AVAuthorizationStatusAuthorized)
		{
            Info(_log, "HyperHDR has the camera's permission");
			 _permission = true;		
        }
        else if (_isAVF)
		{            
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL grantedPerm) {
                
                _permission = grantedPerm;
                
                if (_permission) 
                    Info(_log, "HyperHDR has been granted the camera's permission");
                else
					Error(_log, "HyperHDR has NOT been granted the camera's permission");                

				if (_isAVF)
				{
					if (!_permission)
						QTimer::singleShot(5000, this, SLOT(getPermission()));
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
	QString ret = QString("%1%2").arg(QCoreApplication::applicationDirPath(), "/../lut");
	QFileInfo info(ret);
	ret = info.absoluteFilePath();	
	return ret;
}

void AVFGrabber::loadLutFile(const QString & color)
{	
	
	bool is_yuv = (QString::compare(color, "yuv", Qt::CaseInsensitive) == 0);

	// load lut
	QString fileName1 = QString("%1%2").arg(_configurationPath, "/lut_lin_tables.3d");
	QString fileName2 = QString("%1%2").arg(GetSharedLut(), "/lut_lin_tables.3d");	

	_lutBufferInit = false;

	if (_hdrToneMappingEnabled || is_yuv)
	{
		QString fileName3d = fileName1;
		Debug(_log, "Searching for LUT file in a home folder: '%s'", QSTRING_CSTR(fileName3d));

		FILE* file = fopen(QSTRING_CSTR(fileName3d), "r");

		if (!file)
		{						
			fileName3d = fileName2;
			Debug(_log, "Searching for LUT file in a shared folder: '%s'", QSTRING_CSTR(fileName3d));

			file = fopen(QSTRING_CSTR(fileName3d), "r");
		}
	
		if (file) {
			size_t length;
			Debug(_log, "LUT file found: %s", QSTRING_CSTR(fileName3d));

			fseek(file, 0, SEEK_END);
			length = ftell(file);

			if (length == LUT_FILE_SIZE * 3) {
				unsigned index = 0;
				if (is_yuv && _hdrToneMappingEnabled)
				{
					Debug(_log, "Index 1 for HDR YUV");
					index = LUT_FILE_SIZE;
				}
				else if (is_yuv)
				{
					Debug(_log, "Index 2 for YUV");
					index = LUT_FILE_SIZE * 2;
				}
				else
					Debug(_log, "Index 0 for HDR RGB");
				fseek(file, index, SEEK_SET);

				if (lutBuffer == NULL)
					lutBuffer = (unsigned char*)malloc(length + 4);

				if (fread(lutBuffer, 1, LUT_FILE_SIZE, file) != LUT_FILE_SIZE)
				{
					Error(_log, "Error reading LUT file %s", QSTRING_CSTR(fileName3d));
				}
				else
				{
					_lutBufferInit = true;
					Debug(_log, "LUT file has been loaded");
				}
			}
			else
				Error(_log, "LUT file has invalid length: %i %s. Please generate new one LUT table using the generator page.", length, QSTRING_CSTR(fileName3d));
			fclose(file);
		}
		else
			Error(_log, "LUT file NOT found: %s", QSTRING_CSTR(fileName3d));
	}
}

void AVFGrabber::setFpsSoftwareDecimation(int decimation)
{
	_fpsSoftwareDecimation = decimation;
	Debug(_log,"setFpsSoftwareDecimation to: %i", decimation);
}

int AVFGrabber::getHdrToneMappingEnabled()
{
	return _hdrToneMappingEnabled;
}

void AVFGrabber::setHdrToneMappingEnabled(int mode)
{
	if (_hdrToneMappingEnabled != mode || lutBuffer == NULL)
	{
		_hdrToneMappingEnabled = mode;
		if (lutBuffer!=NULL || !mode)
			Debug(_log,"setHdrToneMappingMode to: %s", (mode == 0) ? "Disabled" : ((mode == 1)? "Fullscreen": "Border mode") );
		else
			Warning(_log,"setHdrToneMappingMode to: enable, but the LUT file is currently unloaded");	
			
		if (_AVFWorkerManager.isActive())
		{
			Debug(_log,"setHdrToneMappingMode replacing LUT and restarting");
			_AVFWorkerManager.Stop();
			if ((_pixelFormat == PixelFormat::YUYV) || (_pixelFormat == PixelFormat::I420) || (_pixelFormat == PixelFormat::NV12))
				loadLutFile("yuv");
			else
				loadLutFile("rgb");				
			_AVFWorkerManager.Start();
		}
	}
	else
		Debug(_log,"setHdrToneMappingMode nothing changed: %s", (mode == 0) ? "Disabled" : ((mode == 1)? "Fullscreen": "Border mode") );
}

AVFGrabber::~AVFGrabber()
{	
	if (lutBuffer!=NULL)
		free(lutBuffer);	
	lutBuffer = NULL;

	uninit();		
}

void AVFGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{
		Debug(_log,"Uninit grabber: %s", QSTRING_CSTR(_deviceName));
		stop();
	}
}

bool AVFGrabber::init()
{
	Debug(_log,"init");
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
		bool    autoDiscovery = (QString::compare(_deviceName, "auto", Qt::CaseInsensitive) == 0 );
		
		getV4Ldevices(true);		

		if (!autoDiscovery && !_deviceProperties.contains(_deviceName))
		{
			Debug(_log, "Device %s is not available. Changing to auto.", QSTRING_CSTR(_deviceName));
			autoDiscovery = true;
		}
		
		if (autoDiscovery)
		{
			Debug(_log, "Forcing auto discovery device");
			if (_deviceProperties.count()>0)
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
		
		
			
		AVFGrabber::DeviceProperties dev = _deviceProperties[foundDevice];
		
		Debug(_log,  "Searching for %s %d x %d @ %d fps (%s)", QSTRING_CSTR(foundDevice), _width, _height,_fps, QSTRING_CSTR(pixelFormatToString(_enc)));
		
		
		
		for( int i=0; i<dev.valid.count() && foundIndex < 0; ++i )		
		{
			bool strict = false;
			const auto& val = dev.valid[i];
			
			if (bestGuess == -1 || (val.x <= bestGuessMinX && val.x >= 640 && val.fps <= bestGuessMinFPS && val.fps >= 10))
			{
				bestGuess = i;
				bestGuessMinFPS = val.fps;
				bestGuessMinX = val.x;
			}
			
			if(_width && _height)
			{				
				strict = true;
				if (val.x != _width || val.y != _height)
					continue;				
			}
					
			if(_fps && _fps!=15)
			{	
				strict = true;
				if (val.fps != _fps)
					continue;
			}
			
			if(_enc != PixelFormat::NO_CHANGE)
			{
				strict = true;
				if (val.pf != _enc)
					continue;
			}
						
			if (strict && (val.fps <= 60 || _fps != 15))
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
		
		if (foundIndex>=0)
		{
			if (init_device(foundDevice, dev.valid[foundIndex]))
				_initialized = true;
		}
		else
			Error(_log, "Could not find any capture device settings");
	}
	

	return _initialized;
}





QString AVFGrabber::FormatRes(int w,int h, QString format)
{
	QString ws = QString::number(w);
	QString hs = QString::number(h);
	
	while (ws.length()<4)
		ws = " " + ws;															
	while (hs.length()<4)
		hs = " " + hs;
	
	return ws+"x"+hs+" "+format;
}

QString AVFGrabber::FormatFrame(int fr)
{
	QString frame = QString::number(fr);
	
	while (frame.length()<2)
		frame = " " + frame;															
	
	return frame;
}

void AVFGrabber::getV4Ldevices()
{
	getV4Ldevices(false);
}

PixelFormat AVFGrabber::identifyFormat(QString format)
{	
	for (int i=0; i<sizeof(fmt_array)/sizeof(fmt_array[0]); i++)
		if (format == fmt_array[i].formatName)
		{
			return fmt_array[i].pixel;
		}

	return PixelFormat::NO_CHANGE;
}

QString AVFGrabber::getIdOfCodec(uint32_t cc4)
{
    QString result="";
    for(uint32_t i=0; i<4; i++)
    {
        result = static_cast<char>(cc4 & 0xFF) + result;
        cc4 >>= 8;
    }
    return result.toUpper();
}

void AVFGrabber::getV4Ldevices(bool silent)
{			
	if (!_isAVF)
	{
		Error(_log, "AVF is not init. Exiting.");
		return;
	}
	
	Debug(_log, "Searching for devices...");


    _deviceProperties.clear();

	AVCaptureDeviceDiscoverySession *session = [AVCaptureDeviceDiscoverySession 
        discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeExternalUnknown]
        mediaType:AVMediaTypeVideo 
        position:AVCaptureDevicePositionUnspecified];

	if (session!=nullptr)
	{
		for (AVCaptureDevice* device in session.devices) 
		{
			AVFGrabber::DeviceProperties properties;			
			properties.name = QString(device.localizedName.UTF8String) + " (" + QString(device.uniqueID.UTF8String) + ")";

			for (AVCaptureDeviceFormat* format in device.formats) 
			{            
				for (AVFrameRateRange* frameRateRange in format.videoSupportedFrameRateRanges) {
					for (int frameRate = frameRateRange.minFrameRate; frameRate <= frameRateRange.maxFrameRate; frameRate++) {
						CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(format.formatDescription);                    
						uint32_t cc4=CMFormatDescriptionGetMediaSubType(format.formatDescription);

						QString sFormat=getIdOfCodec(cc4);					
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
								Debug(_log,  "%s %d x %d @ %d fps %s (%s)", QSTRING_CSTR(properties.name), di.x, di.y, di.fps, QSTRING_CSTR(sFormat), QSTRING_CSTR(pixelFormatToString(di.pf)));
							else
								Debug(_log,  "%s %d x %d @ %d fps %s (unsupported)", QSTRING_CSTR(properties.name), di.x, di.y, di.fps, QSTRING_CSTR(sFormat));
						}
					}
				}            
			}        
			_deviceProperties.insert(properties.name, properties);
		}
	}	
}

void AVFGrabber::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	_noSignalThresholdColor.red   = uint8_t(255*redSignalThreshold);
	_noSignalThresholdColor.green = uint8_t(255*greenSignalThreshold);
	_noSignalThresholdColor.blue  = uint8_t(255*blueSignalThreshold);
	_noSignalCounterThreshold     = qMax(1, noSignalCounterThreshold);

	Info(_log, "Signal threshold set to: {%d, %d, %d} and frames: %d", _noSignalThresholdColor.red, _noSignalThresholdColor.green, _noSignalThresholdColor.blue, _noSignalCounterThreshold );
}

void AVFGrabber::setSignalDetectionOffset(double horizontalMin, double verticalMin, double horizontalMax, double verticalMax)
{
	// rainbow 16 stripes 0.47 0.2 0.49 0.8
	// unicolor: 0.25 0.25 0.75 0.75

	_x_frac_min = horizontalMin;
	_y_frac_min = verticalMin;
	_x_frac_max = horizontalMax;
	_y_frac_max = verticalMax;

	Info(_log, "Signal detection area set to: %f,%f x %f,%f", _x_frac_min, _y_frac_min, _x_frac_max, _y_frac_max );
}

void	AVFGrabber::ResetCounter(uint64_t from)
{
	frameStat.frameBegin = from;
	frameStat.averageFrame = 0;
	frameStat.badFrame = 0;
	frameStat.goodFrame = 0;
	frameStat.segment = 0;
}

bool AVFGrabber::start()
{
	
	try
	{
		ResetCounter(QDateTime::currentMSecsSinceEpoch());
		_AVFWorkerManager.Start();
				
		if (_AVFWorkerManager.workersCount<=1)
			Info(_log, "Multithreading for AVF is disabled. Available thread's count %d", _AVFWorkerManager.workersCount );
		else
			Info(_log, "Multithreading for AVF is enabled. Available thread's count %d", _AVFWorkerManager.workersCount );
		
		if (init())
		{			
			start_capturing();
			Info(_log, "Started");
			return true;
		}
	}
	catch(std::exception& e)
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
		_deviceProperties.clear();
		_initialized = false;
		Info(_log, "Stopped");
	}
}



bool AVFGrabber::init_device(QString deviceName, DevicePropertiesItem props)
{
	bool result = false;
	bool notFound = true;
	PixelFormat pixelformat = props.pf;;
	QString sFormat = pixelFormatToString(pixelformat), error;
	QString guid = _deviceProperties[deviceName].name;
	CMTime minFrameDuration;
	
	Debug(_log,  "Init_device: %s, %d x %d @ %d fps (%s) => %s", QSTRING_CSTR(deviceName), props.x, props.y, props.fps, QSTRING_CSTR(sFormat), QSTRING_CSTR(guid));

	
	for (AVCaptureDevice* device in [AVCaptureDeviceDiscoverySession 
										discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeExternalUnknown]
										mediaType:AVMediaTypeVideo 
										position:AVCaptureDevicePositionUnspecified].devices) 
	{			
		QString devName  = QString(device.localizedName.UTF8String) + " (" + QString(device.uniqueID.UTF8String) + ")";
		
		if (devName == deviceName && notFound)
		{
			for (AVCaptureDeviceFormat* format in device.formats)
				if (notFound)
				{
					uint32_t cc4=CMFormatDescriptionGetMediaSubType(format.formatDescription);
					if (props.fourcc != cc4)
						continue;

					for (AVFrameRateRange* frameRateRange in format.videoSupportedFrameRateRanges) 						
						if (notFound)
						{
							CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
							int minF=frameRateRange.minFrameRate, maxF=frameRateRange.maxFrameRate;
							uint32_t cc4=CMFormatDescriptionGetMediaSubType(format.formatDescription);
							QString sFormat=getIdOfCodec(cc4);			
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
							Error(_log,  "Listener creation failed");
						}
						else
						{							
							_avfDelegate->_nativeSession = [AVCaptureSession new];

							NSError* apiError = nil;
							AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&apiError];

							if (!input) 
							{
								Error(_log,  "Could not open video capturing device: %s", apiError.localizedDescription.UTF8String);        
							}
							else
							{
								Info(_log,  "Opening: %s", QSTRING_CSTR(deviceName));

								[_avfDelegate->_nativeSession addInput:input];

								[device lockForConfiguration:NULL];
								device.activeFormat = format;
								
								@try {
									[device setActiveVideoMinFrameDuration:minFrameDuration];
								}
								@catch (NSException *exception) {
									Warning(_log,  "Device doesn't support setting min framerate");  
								}
								@try {
									[device setActiveVideoMaxFrameDuration:minFrameDuration];
								}
								@catch (NSException *exception) {
									Warning(_log,  "Device doesn't support setting max framerate");  
								}

								AVCaptureVideoDataOutput* output = [AVCaptureVideoDataOutput new];

								[_avfDelegate->_nativeSession addOutput:output];
								output.videoSettings = nil;

								switch (pixelformat)
								{			
									case PixelFormat::YUYV:
									{
										output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
														[NSNumber numberWithUnsignedInt:kCMPixelFormat_422YpCbCr8_yuvs], (id)kCVPixelBufferPixelFormatTypeKey,
														nil];
									}
									break;
									case PixelFormat::NV12:
									{
										output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
														[NSNumber numberWithUnsignedInt:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange], (id)kCVPixelBufferPixelFormatTypeKey,
														nil];
									}
									break;
									case PixelFormat::MJPEG:
									{
										output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
														[NSNumber numberWithUnsignedInt:kCMPixelFormat_422YpCbCr8_yuvs], (id)kCVPixelBufferPixelFormatTypeKey,
														nil];
										pixelformat = PixelFormat::YUYV;
										Warning(_log,  "Let macOS do accelerated MJPEG decoding to YUYV");
									}
									break;
									default:
									{
										Error(_log,  "Unsupported encoding");					
									}
								}				

								if (output.videoSettings!=nil)
								{
									_pixelFormat = pixelformat;
									_width = props.x;
									_height = props.y;		
		
									switch (_pixelFormat)
									{			

										case PixelFormat::YUYV:
										{
											loadLutFile("yuv");		
											_frameByteSize = props.x * props.y * 2;
											_lineLength = props.x * 2;
										}
										break;			

										case PixelFormat::NV12:
										{
											loadLutFile("yuv");
											_frameByteSize = (6 * props.x * props.y) / 4;
											_lineLength = props.x;
										}
										break;	

										default:
										{
											Error(_log,  "Unsupported encoding");
											output.videoSettings = nil;
										}
									}		
								}

								if (output.videoSettings!=nil)
								{
									[output setAlwaysDiscardsLateVideoFrames:true];		
		
									_avfDelegate->_avfGrabber = this;
									_avfDelegate->_sequencer = dispatch_queue_create("VideoDataOutputQueue", DISPATCH_QUEUE_SERIAL);
									[output setSampleBufferDelegate:_avfDelegate queue:_avfDelegate->_sequencer];

									if (!(_avfDelegate->_nativeSession.running))
									{
										[_avfDelegate->_nativeSession startRunning];
										Info(_log,  "AVF video grabber starts capturing");
									}			
		
									[device unlockForConfiguration];

									Info(_log,  "AVF video grabber initialized successfully");
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
		Warning(_log,  "Could not found video grabber configuration");
	}	

	if (!result)
	{
		uninit_device();
	}
	
	return result;
}

void AVFGrabber::uninit_device()
{
	if (_avfDelegate!=nullptr && _avfDelegate->_nativeSession  != nullptr)
	{
		[_avfDelegate->_nativeSession stopRunning];
		[_avfDelegate->_nativeSession release];
		_avfDelegate->_nativeSession = nullptr;
		_avfDelegate->_sequencer = nullptr;		
	}
	_avfDelegate = nullptr;
	Info(_log,  "AVF video grabber uninit");
}

void AVFGrabber::start_capturing()
{
	if (_avfDelegate != nullptr && _avfDelegate->_nativeSession != nullptr)
	{
		if (_avfDelegate->_nativeSession.running)
		{			
			Info(_log,  "AVF video grabber already capturing");
		}
		else
			Warning(_log,  "AVF video grabber cant start capturing");		
	}
}

void AVFGrabber::receive_image(const void *frameImageBuffer, int size, QString message)
{
	if (frameImageBuffer == NULL || size ==0)
		Error(_log,  "Received empty image frame: %s", QSTRING_CSTR(message));
	else
	{
		if (!message.isEmpty())
			Debug(_log,  "Received image frame: %s", QSTRING_CSTR(message));
		process_image(frameImageBuffer, size);
	}	
}

bool AVFGrabber::process_image(const void *frameImageBuffer, int size)
{	
	bool frameSend = false;
	
	unsigned int processFrameIndex = _currentFrame++;
		
	// frame skipping
	if ( (processFrameIndex % _fpsSoftwareDecimation != 0) && (_fpsSoftwareDecimation > 1))
		return frameSend;		

	// We do want a new frame...
	if (size < _frameByteSize && _pixelFormat != PixelFormat::MJPEG)
	{
		Error(_log, "Frame too small: %d != %d", size, _frameByteSize);
	}
	else
	{
		if (_AVFWorkerManager.isActive())
		{		
			// benchmark
			uint64_t currentTime= QDateTime::currentMSecsSinceEpoch();
			long	 diff = currentTime - frameStat.frameBegin;
			if ( diff >=1000*60)
			{
				int total = (frameStat.badFrame+frameStat.goodFrame);
				int av = (frameStat.goodFrame>0)?frameStat.averageFrame/frameStat.goodFrame:0;
				Info(_log, "Video FPS: %.2f, av. delay: %dms, good: %d, bad: %d (%.2f,%d)", 
					total/60.0,
					av,
					frameStat.goodFrame,
					frameStat.badFrame,
					diff/1000.0,
					frameStat.segment);

				ResetCounter(currentTime);				
			}
			
			if (_AVFWorkerManager.workers == nullptr)
			{	
				_AVFWorkerManager.InitWorkers();
				Debug(_log, "Worker's thread count  = %d", _AVFWorkerManager.workersCount);
				
				for (unsigned int i=0; i < _AVFWorkerManager.workersCount && _AVFWorkerManager.workers != nullptr; i++)
				{
					AVFWorker* _workerThread= _AVFWorkerManager.workers[i];
					connect(_workerThread, SIGNAL(newFrameError(unsigned int, QString,unsigned int)), this , SLOT(newWorkerFrameError(unsigned int, QString,unsigned int)));
					connect(_workerThread, SIGNAL(newFrame(unsigned int, const Image<ColorRgb> &,unsigned int,quint64)), this , SLOT(newWorkerFrame(unsigned int, const Image<ColorRgb> &, unsigned int,quint64)));
				}
		    }	 
		    	
			
			
			for (unsigned int i=0; _AVFWorkerManager.isActive() &&
						i < _AVFWorkerManager.workersCount &&
				_AVFWorkerManager.workers != nullptr; i++)
			{													
				if ((_AVFWorkerManager.workers[i]->isFinished() || !_AVFWorkerManager.workers[i]->isRunning()))
					// aquire lock
					if (_AVFWorkerManager.workers[i]->isBusy() == false)
					{		
						AVFWorker* _workerThread = _AVFWorkerManager.workers[i];
						frameStat.segment|=(1<<(i));
						
						if ((_pixelFormat==PixelFormat::YUYV || _pixelFormat == PixelFormat::I420 ||
							_pixelFormat == PixelFormat::NV12)  && !_lutBufferInit)
						{														
							if (lutBuffer == NULL)
								lutBuffer = (unsigned char *)malloc(LUT_FILE_SIZE + 4);

							if (lutBuffer != NULL)
							{
								for (int y = 0; y < 256; y++)
									for (int u = 0; u < 256; u++)
										for (int v = 0; v < 256; v++)
										{
											uint32_t ind_lutd = LUT_INDEX(y, u, v);
											ColorSys::yuv2rgb(y, u, v,
												lutBuffer[ind_lutd],
												lutBuffer[ind_lutd + 1],
												lutBuffer[ind_lutd + 2]);
										}
								_lutBufferInit = true;
							}
																	
							Error(_log,"You have forgotten to put lut_lin_tables.3d file in the HyperHdr configuration folder. Internal LUT table for YUV conversion has been created instead.");
						}			
						
						_workerThread->setup(
							i, 														
							_pixelFormat,
							(uint8_t *)frameImageBuffer, size, _width, _height, _lineLength,				
							_subsamp, 				
							_cropLeft,  _cropTop, _cropBottom, _cropRight, 		
							processFrameIndex,currentTime,_hdrToneMappingEnabled,
							(_lutBufferInit)? lutBuffer: NULL, _qframe);							
									
						if (_AVFWorkerManager.workersCount>1)
							_AVFWorkerManager.workers[i]->start();
						else
							_AVFWorkerManager.workers[i]->startOnThisThread();
						//Debug(_log, "Frame index = %d => send to decode to the thread = %i", processFrameIndex,i);			
						frameSend = true;
						break;		
					}
			}						
		}		
	}

	return frameSend;
	
}

void AVFGrabber::newWorkerFrameError(unsigned int workerIndex, QString error, unsigned int sourceCount)
{
	
	frameStat.badFrame++;
	//Debug(_log, "Error occured while decoding mjpeg frame %d = %s", sourceCount, QSTRING_CSTR(error));	
	
	// get next frame	
	if (workerIndex > _AVFWorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);	
				
	if (workerIndex <= _AVFWorkerManager.workersCount)
		_AVFWorkerManager.workers[workerIndex]->noBusy();
	
}


void AVFGrabber::newWorkerFrame(unsigned int workerIndex, const Image<ColorRgb>& image, unsigned int sourceCount, quint64 _frameBegin)
{
	
	frameStat.goodFrame++;
	frameStat.averageFrame += QDateTime::currentMSecsSinceEpoch() - _frameBegin;
	
	//Debug(_log, "Frame index = %d <= received from the thread and it's ready", sourceCount);	
		
	checkSignalDetectionEnabled(image);
	
	// get next frame	
	if (workerIndex > _AVFWorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);	
				
	if (workerIndex <= _AVFWorkerManager.workersCount)
		_AVFWorkerManager.workers[workerIndex]->noBusy();
	
}

void AVFGrabber::checkSignalDetectionEnabled(Image<ColorRgb> image)
{
	if (_signalDetectionEnabled)
	{
		// check signal (only in center of the resulting image, because some grabbers have noise values along the borders)
		bool noSignal = true;

		// top left
		unsigned xOffset  = image.width()  * _x_frac_min;
		unsigned yOffset  = image.height() * _y_frac_min;

		// bottom right
		unsigned xMax     = image.width()  * _x_frac_max;
		unsigned yMax     = image.height() * _y_frac_max;


		for (unsigned x = xOffset; noSignal && x < xMax; ++x)
		{
			for (unsigned y = yOffset; noSignal && y < yMax; ++y)
			{
				noSignal &= (ColorRgb&)image(x, y) <= _noSignalThresholdColor;
			}
		}

		if (noSignal)
		{
			++_noSignalCounter;
		}
		else
		{
			if (_noSignalCounter >= _noSignalCounterThreshold)
			{
				_noSignalDetected = true;
				Info(_log, "Signal detected");
			}

			_noSignalCounter = 0;
		}

		if ( _noSignalCounter < _noSignalCounterThreshold)
		{
			emit newFrame(image);
		}
		else if (_noSignalCounter == _noSignalCounterThreshold)
		{
			_noSignalDetected = false;
			Info(_log, "Signal lost");
		}
	}
	else
	{
		emit newFrame(image);
	}
}

void AVFGrabber::setSignalDetectionEnable(bool enable)
{
	if (_signalDetectionEnabled != enable)
	{
		_signalDetectionEnabled = enable;
		Info(_log, "Signal detection is now %s", enable ? "enabled" : "disabled");
	}
}

void AVFGrabber::setDeviceVideoStandard(QString device)
{
	QString olddeviceName = _deviceName;
	if (_deviceName != device)
	{
		Debug(_log,"setDeviceVideoStandard preparing to restart AVF grabber. Old: '%s' new: '%s'",QSTRING_CSTR(_deviceName) , QSTRING_CSTR(device));
		
		_deviceName = device;

		if (!olddeviceName.isEmpty())
		{
			Debug(_log,"Restarting AVF grabber for new device");
			uninit();
			
			start();
		}
		
	}
}

bool AVFGrabber::setInput(int input)
{
	return true;	
}

bool AVFGrabber::setWidthHeight(int width, int height)
{
	if(Grabber::setWidthHeight(width,height))
	{
		Debug(_log,"setWidthHeight preparing to restarting AVF grabber %i",_initialized);
		
		if (_initialized)
		{
			Debug(_log,"Restarting AVF grabber");
			uninit();
			start();
		}
		return true;
	}
	return false;
}

bool AVFGrabber::setFramerate(int fps)
{
	if(Grabber::setFramerate(fps))
	{
		if (_initialized)
		{
			Debug(_log,"Restarting AVF grabber");
			uninit();
			start();
		}
		return true;
	}
	return false;
}

QStringList AVFGrabber::getV4L2devices() const
{	
	QStringList result = QStringList();
	for (auto it = _deviceProperties.begin(); it != _deviceProperties.end(); ++it)
	{
		result << it.key();
	}
	return result;	
}

QString AVFGrabber::getV4L2deviceName(const QString& devicePath) const
{		
	return devicePath;
}

QMultiMap<QString, int> AVFGrabber::getV4L2deviceInputs(const QString& devicePath) const
{	
	return _deviceProperties.value(devicePath).inputs;
}

QStringList AVFGrabber::getResolutions(const QString& devicePath) const
{
	return _deviceProperties.value(devicePath).displayResolutions;
}

QStringList AVFGrabber::getFramerates(const QString& devicePath) const
{
	return _deviceProperties.value(devicePath).framerates;
}


void AVFGrabber::setEncoding(QString enc)
{
	PixelFormat _oldEnc = _enc;
	
	_enc = parsePixelFormat(enc);	
	Debug(_log,"Force encoding to: %s (old: %s)", QSTRING_CSTR(pixelFormatToString(_enc)), QSTRING_CSTR(pixelFormatToString(_oldEnc)));
			
	if (_oldEnc != _enc)
	{
		if (_initialized)
		{
			Debug(_log,"Restarting AVF grabber");
			uninit();
			start();
		}
	}
}

void AVFGrabber::setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue)
{
	if (_brightness != brightness || _contrast != contrast || _saturation != saturation || _hue != hue)
	{		
		_brightness = brightness;
		_contrast = contrast;
		_saturation = saturation;
		_hue = hue;
		
		Debug(_log,"Set brightness to %i, contrast to %i, saturation to %i, hue to %i", _brightness, _contrast, _saturation, _hue);
						
		if (_initialized)
		{
			Debug(_log,"Restarting AV MEDIAFOUNDATION grabber");
			uninit();
			start();
		}
	}
	else
		Debug(_log,"setBrightnessContrastSaturationHue nothing changed");
}

void AVFGrabber::setQFrameDecimation(int setQframe)
{
	_qframe = setQframe;
	Info(_log, QSTRING_CSTR(QString("setQFrameDecimation is now: %1").arg(_qframe ? "enabled" : "disabled")));
}

