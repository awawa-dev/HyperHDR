#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <mferror.h>
#include <strmif.h>

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
//#include <sys/time.h>
//#include <sys/mman.h>
//#include <sys/ioctl.h>
#include <limits.h>

#include <hyperion/Hyperion.h>
#include <hyperion/HyperionIManager.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include "grabber/QTCGrabber.h"
#include "utils/ColorSys.h"




#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "mf.lib")
#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "mfreadwrite.lib")


#define CHECK(hr) SUCCEEDED(hr)
#define CLEAR(x) memset(&(x), 0, sizeof(x))

#ifndef V4L2_CAP_META_CAPTURE
#define V4L2_CAP_META_CAPTURE 0x00800000 // Specified in kernel header v4.16. Required for backward compatibility.
#endif

// some stuff for HDR tone mapping
#define LUT_FILE_SIZE 50331648

class SourceReaderCB : public IMFSourceReaderCallback
	{
	public:
		SourceReaderCB(QTCGrabber* grabber) : 
		  m_nRefCount(1), _grabber(grabber), m_bEOS(FALSE), m_hrStatus(S_OK)
		{
			InitializeCriticalSection(&m_critsec);
		}

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
		{
			static const QITAB qit[] =
			{
				QITABENT(SourceReaderCB, IMFSourceReaderCallback),
				{ 0 },
			};
			return QISearch(this, qit, iid, ppv);
		}
		STDMETHODIMP_(ULONG) AddRef()
		{
			return InterlockedIncrement(&m_nRefCount);
		}
		STDMETHODIMP_(ULONG) Release()
		{
			ULONG uCount = InterlockedDecrement(&m_nRefCount);
			if (uCount == 0)
			{
				delete this;
			}
			return uCount;
		}

		// IMFSourceReaderCallback methods
		STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
			DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample)			
		{
			
			EnterCriticalSection(&m_critsec);

			if (SUCCEEDED(hrStatus))
			{
				QString error = "";
				bool frameSend = false;
				
				if (pSample)
				{
					IMFMediaBuffer* buffer;
					
					hrStatus = pSample->ConvertToContiguousBuffer(&buffer);
					if (SUCCEEDED(hrStatus))
					{

						BYTE* data = nullptr; 
						DWORD maxLength = 0, currentLength = 0;
						
						hrStatus = buffer->Lock(&data, &maxLength, &currentLength);
						if(SUCCEEDED(hrStatus))
						{
							frameSend = true;
							
							//error = "Size "+ QString::number(currentLength) + "x" + QString::number(maxLength);				
							_grabber->receive_image(data,currentLength,error);
							
							buffer->Unlock();														
						}
						else
							error = QString("buffer->Lock failed => %1").arg(hrStatus);
						
						buffer->Release();
					}
					else
						error = QString("pSample->ConvertToContiguousBuffer failed => %1").arg(hrStatus);
					
				}
				else
					error = "pSample is NULL";
				
				if (!frameSend)
					_grabber->receive_image(NULL,0,error);
			}
			else
			{
				// Streaming error.
				NotifyError(hrStatus);				
			}

			if (MF_SOURCE_READERF_ENDOFSTREAM & dwStreamFlags)
			{
				// Reached the end of the stream.
				m_bEOS = TRUE;
			}
			m_hrStatus = hrStatus;

			LeaveCriticalSection(&m_critsec);			
			return S_OK;
		}				
			

		STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *)
		{
			return S_OK;
		}

		STDMETHODIMP OnFlush(DWORD)
		{
			return S_OK;
		}

	public:
		HRESULT Wait(DWORD dwMilliseconds, BOOL *pbEOS)
		{			
			return m_hrStatus;
		}
		
	private:
		
		// Destructor is private. Caller should call Release.
		virtual ~SourceReaderCB() 
		{
		}

		void NotifyError(HRESULT hr)
		{
			wprintf(L"Source Reader error: 0x%X\n", hr);
		}

	private:
		long                m_nRefCount;        // Reference count.
		CRITICAL_SECTION    m_critsec;
		QTCGrabber*         _grabber;
		BOOL                m_bEOS;
		HRESULT             m_hrStatus;

};

QTCGrabber::format_t fmt_array[] =
	{
		{ MFVideoFormat_RGB32,	"RGB32", PixelFormat::RGB32 },
		{ MFVideoFormat_ARGB32,"ARGB32", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_RGB24,	"RGB24", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_RGB555,"RGB555", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_RGB565,"RGB565", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_RGB8,	"RGB8", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_AI44,	"AI44", PixelFormat::NO_CHANGE},
		{ MFVideoFormat_AYUV,	"AYUV", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_YUY2,  "YUY2", PixelFormat::YUYV },
		{ MFVideoFormat_YVYU,  "YVYU", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_YVU9,  "YVU9", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_UYVY,  "UYVY", PixelFormat::UYVY },
		{ MFVideoFormat_NV11,  "NV11", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_NV12,  "NV12", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_YV12,  "YV12", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_I420,  "I420", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_IYUV,  "IYUV", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_Y210,  "Y210", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_Y216,  "Y216", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_Y410,  "Y410", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_Y416,  "Y416", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_Y41P,  "Y41P", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_Y41T,  "Y41T", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_Y42T,  "Y42T", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_P210,  "P210", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_P216,  "P216", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_P010,  "P010", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_P016,  "P016", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_v210,  "v210", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_v216,  "v216", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_v410,  "v410", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_MP43,  "MP43", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_MP4S,  "MP4S", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_M4S2,  "M4S2", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_MP4V,  "MP4V", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_WMV1,  "WMV1", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_WMV2,  "WMV2", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_WMV3,  "WMV3", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_WVC1,  "WVC1", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_MSS1,  "MSS1", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_MSS2,  "MSS2", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_MPG1,  "MPG1", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_DVSL,  "DVSL", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_DVSD,  "DVSD", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_DVHD,  "DVHD", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_DV25,  "DV25", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_DV50,  "DV50", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_DVH1,  "DVH1", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_DVC,   "DVC" , PixelFormat::NO_CHANGE },
		{ MFVideoFormat_H264,  "H264", PixelFormat::NO_CHANGE },
		{ MFVideoFormat_MJPG,  "MJPG", PixelFormat::MJPEG }
	};

QTCGrabber::QTCGrabber(const QString & device
		, unsigned width
		, unsigned height
		, unsigned fps
		, unsigned input
		, VideoStandard videoStandard
		, PixelFormat pixelFormat
		, const QString & configurationPath
		)
	: Grabber("V4L2:QTC:"+device)
	, _deviceName()
	, _videoStandard(videoStandard)
	, _ioMethod(IO_METHOD_MMAP)
	, _fileDescriptor(-1)
	, _buffers()
	, _pixelFormat(pixelFormat)
	, _lineLength(-1)
	, _frameByteSize(-1)
	, _noSignalCounterThreshold(40)
	, _noSignalThresholdColor(ColorRgb{0,0,0})
	, _signalDetectionEnabled(true)
	, _cecDetectionEnabled(true)
	, _cecStandbyActivated(false)
	, _noSignalDetected(false)
	, _noSignalCounter(0)
	, _x_frac_min(0.25)
	, _y_frac_min(0.25)
	, _x_frac_max(0.75)
	, _y_frac_max(0.75)
	, _initialized(false)
	, _deviceAutoDiscoverEnabled(false)
	,_hdrToneMappingEnabled(0)	
	, _fpsSoftwareDecimation(1)
	, lutBuffer(NULL)
	, _lutBufferInit(false)
	, _currentFrame(0)
	, _configurationPath(configurationPath)
	, _enc(pixelFormat)
	, READER(NULL)
	, _brightness(0)
	, _contrast(0)
	
{	
	// init
	_isMF = false;
	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (!CHECK(hr))
	{			
		CoUninitialize();			
	}
	else
	{
		_sourceReaderCB = new SourceReaderCB(this);
		_isMF = true;
	}

		
	// devices
	getV4Ldevices();

	// init
	setInput(input);
	setWidthHeight(width, height);
	setFramerate(fps);
	setDeviceVideoStandard(device, videoStandard);
	Debug(_log,"Init pixel format: %i", static_cast<int>(_pixelFormat));	
}

QString QTCGrabber::GetSharedLut()
{	
	
	return QCoreApplication::applicationDirPath();
}

void QTCGrabber::loadLutFile(const QString & color)
{	
	
	bool is_yuv = (QString::compare(color, "yuv", Qt::CaseInsensitive) == 0);
	
	// load lut
	QString fileName1 = QString("%1%2").arg(_configurationPath,"/lut_lin_tables.3d");
	QString fileName2 = QString("%1%2").arg(GetSharedLut(),"/lut_lin_tables.3d");
	Debug(_log,"loadLutFile: '%s'", QSTRING_CSTR(fileName1));	
	
	_lutBufferInit = false;
	
	if (_hdrToneMappingEnabled || is_yuv)
	{
		QString fileName3d = fileName1;
		FILE *file;
		errno_t err;
		
		err = fopen_s(&file, QSTRING_CSTR(fileName3d), "rb");
		
		if( err != 0 )
		{
			Debug(_log,"LUT table: trying distro file location");
			fileName3d = fileName2;
			err = fopen_s(&file, QSTRING_CSTR(fileName3d), "rb");
		}
				
		if( err == 0 )
		{
			size_t length;
			Debug(_log,"LUT file found: %s", QSTRING_CSTR(fileName3d));
			
			fseek(file, 0, SEEK_END);
			length = ftell(file);
									
			if (length ==  LUT_FILE_SIZE*3) {
				unsigned index = 0;
				if (is_yuv && _hdrToneMappingEnabled)
				{
					Debug(_log,"Index 1 for HDR YUV");
					index = LUT_FILE_SIZE;
				}
				else if (is_yuv)
				{
					Debug(_log,"Index 2 for YUV");				
					index = LUT_FILE_SIZE*2;
				}
				else 
					Debug(_log,"Index 0 for HDR RGB");								
				fseek(file, index, SEEK_SET);
			
				if (lutBuffer==NULL)
					lutBuffer = (unsigned char *)malloc(length + 1);
					
				if(fread(lutBuffer, 1, LUT_FILE_SIZE, file) != LUT_FILE_SIZE)
				{					
					Error(_log,"Error reading LUT file %s", QSTRING_CSTR(fileName3d));
				}
				else
				{
					_lutBufferInit = true;
					Debug(_log,"LUT file has been loaded");					
				}
			}
			else
				Error(_log,"LUT file has invalid length: %i %s. Please generate new one LUT table using the generator page.", length, QSTRING_CSTR(fileName3d));
			fclose(file);
		}
		else
			Error(_log,"LUT file NOT found: %s", QSTRING_CSTR(fileName3d));
	}					
}

void QTCGrabber::setFpsSoftwareDecimation(int decimation)
{
	_fpsSoftwareDecimation = decimation;
	Debug(_log,"setFpsSoftwareDecimation to: %i", decimation);
}

void QTCGrabber::setHdrToneMappingEnabled(int mode)
{
	_hdrToneMappingEnabled = mode;
	if (lutBuffer!=NULL || !mode)
		Debug(_log,"setHdrToneMappingMode to: %s", (mode == 0) ? "Disabled" : ((mode == 1)? "Fullscreen": "Border mode") );
	else
		Warning(_log,"setHdrToneMappingMode to: enable, but the LUT file is currently unloaded");	
		
	if (_QTCWorkerManager.isActive())	
	{
		Debug(_log,"setHdrToneMappingMode replacing LUT");
		_QTCWorkerManager.Stop();
		if ((_pixelFormat == PixelFormat::UYVY) || (_pixelFormat == PixelFormat::YUYV))
			loadLutFile("yuv");
		else
			loadLutFile("rgb");				
		_QTCWorkerManager.Start();
	}
}

QTCGrabber::~QTCGrabber()
{	
	if (lutBuffer!=NULL)
		free(lutBuffer);	
	lutBuffer = NULL;

	uninit();
	
	// release mf
	if (_isMF)
	{
		if (READER != NULL)
		{
			READER->Release();
			READER = NULL;
		}
		
		if (_sourceReaderCB!=NULL)
		{
			_sourceReaderCB->Release();
			_sourceReaderCB = NULL;
		}
		
		MFShutdown();
		CoUninitialize();
	}
}

void QTCGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{
		Debug(_log,"uninit grabber: %s", QSTRING_CSTR(_deviceName));
		stop();
	}
}

bool QTCGrabber::init()
{
	Debug(_log,"init");	
	
	
	if (!_initialized && _isMF)
	{
		QString foundDevice = "";
		int     foundIndex = -1;
		int     bestGuess = -1;
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
		
		
			
		QTCGrabber::DeviceProperties dev = _deviceProperties[foundDevice];
		
		Debug(_log,  "Searching for %s %d x %d @ %d fps (%s)", QSTRING_CSTR(foundDevice), _width, _height,_fps, QSTRING_CSTR(pixelFormatToString(_enc)));
		
		
		
		for( int i=0; i<dev.valid.count() && foundIndex < 0; ++i )		
		{
			auto val = dev.valid[i];
			
			if (bestGuess==-1)
				bestGuess = i;
			
			if(_width && _height)
			{				
				if (val.x != _width || val.y != _height)
					continue;				
			}
					
			if(_fps && _fps!=15)
			{							
				if (val.fps != _fps)
					continue;
			}
			
			if(_enc != PixelFormat::NO_CHANGE)
			{
				if (val.pf != _enc)
					continue;
			}
						
			foundIndex = i;
		}
		
		if (foundIndex < 0 && bestGuess >= 0)
		{
			Debug(_log, "Forcing best guess");
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



QString QTCGrabber::identify_format(const GUID format, PixelFormat& pixelformat)
{	
	for (int i=0; i<sizeof(fmt_array)/sizeof(fmt_array[0]); i++)
		if (IsEqualGUID(format, fmt_array[i].format_id))
		{
			pixelformat = fmt_array[i].pixel ;
			if (fmt_array[i].pixel == PixelFormat::NO_CHANGE )
				return fmt_array[i].format_name +" <unsupported>";
			else
				return fmt_array[i].format_name;
		}

	return "<unsupported>";
}

QString QTCGrabber::FormatRes(int w,int h, QString format)
{
	QString ws = QString::number(w);
	QString hs = QString::number(h);
	
	while (ws.length()<4)
		ws = " " + ws;															
	while (hs.length()<4)
		hs = " " + hs;
	
	return ws+"x"+hs+" "+format;
}

QString QTCGrabber::FormatFrame(int fr)
{
	QString frame = QString::number(fr);
	
	while (frame.length()<2)
		frame = " " + frame;															
	
	return frame;
}

void QTCGrabber::getV4Ldevices()
{
	getV4Ldevices(false);
}

void QTCGrabber::getV4Ldevices(bool silent)
{	
	QStringList result;	
	HRESULT hr;
	UINT32 count;
    IMFActivate** devices;    
	IMFAttributes* attr;
	
	if (!_isMF) {
		Error(_log, "getV4Ldevices(): MF not init");
		return;
	}
	else
		Debug(_log, "getV4Ldevices()");
	
	_deviceProperties.clear();
	hr = MFCreateAttributes(&attr, 1);
	if(SUCCEEDED(hr))
	{
		hr = attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
		if(SUCCEEDED(hr))
		{
			hr = MFEnumDeviceSources(attr, &devices, &count);				
			if(SUCCEEDED(hr))
			{
				Debug(_log, "Detected %u devices", count);

				for (UINT32 i = 0; i < count; i++)
				{
					UINT32 length;
					LPWSTR name;
					LPWSTR symlink;

					hr = devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &length);
					if(SUCCEEDED(hr))
					{
						hr = devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symlink, &length);
						if(SUCCEEDED(hr))
						{
							QString dev = QString::fromUtf16((const ushort*)name);
							IMFMediaType *pType = NULL;
							IMFMediaSource *pSource = NULL;
							
								
							QTCGrabber::DeviceProperties properties;
							properties.name = QString::fromUtf16((const ushort*)symlink);
								
							Debug(_log, "Found capture device: %s", QSTRING_CSTR(dev));	
							hr = devices[i]->ActivateObject(IID_PPV_ARGS(&pSource));
							if(SUCCEEDED(hr))
							{
								IMFSourceReader* reader;
        
								hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &reader);
								
								if(SUCCEEDED(hr))
								{
									for (DWORD i = 0; ; i++)
									{										
										GUID format;
										UINT32 interlace_mode = 0;
										UINT64 frame_size;
										UINT64 frame_rate;

										hr = reader->GetNativeMediaType(
											(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
											i,
											&pType
											);

										if (FAILED(hr)) { break; }

										hr = pType->GetGUID(MF_MT_SUBTYPE, &format);
										if(SUCCEEDED(hr))
										{
											hr = pType->GetUINT64(MF_MT_FRAME_SIZE, &frame_size);
											if(SUCCEEDED(hr))
											{
												hr = pType->GetUINT64(MF_MT_FRAME_RATE, &frame_rate);
												if(SUCCEEDED(hr))
												{
													hr = pType->GetUINT32(MF_MT_INTERLACE_MODE, &interlace_mode);
													if(SUCCEEDED(hr) && interlace_mode == MFVideoInterlace_Progressive && frame_rate>0)
													{
														PixelFormat pixelformat;
														DWORD w = frame_size >> 32;
														DWORD h = (DWORD) frame_size;
														DWORD fr1 = frame_rate >> 32;
														DWORD fr2 = (DWORD) frame_rate;
														QString sFormat = identify_format(format, pixelformat);
														if (!sFormat.contains("unsupported"))
														{
															
															int framerate = fr1/fr2;
															QString sFrame = FormatFrame(framerate);
															QString resolution = FormatRes(w,h,sFormat);
															QString displayResolutions = FormatRes(w,h,"");
																																												

															if (!properties.displayResolutions.contains(displayResolutions))
																properties.displayResolutions << displayResolutions;
															
															if (!properties.framerates.contains(sFrame))
																properties.framerates << sFrame;
															
															DevicePropertiesItem di; 			
															di.x = w;
															di.y = h;
															di.fps = framerate;
															di.fps_a = fr1;
															di.fps_b = fr2;
															di.pf = pixelformat;
															di.guid = format;
															properties.valid.append(di);
															if (!silent)
																Debug(_log,  "*%s %d x %d @ %d fps %s (%s)", QSTRING_CSTR(dev), di.x, di.y, di.fps, QSTRING_CSTR(sFormat),QSTRING_CSTR(pixelFormatToString(di.pf)));
														}
														//else
														//	Debug(_log,  "%s %d x %d @ %d fps %s", QSTRING_CSTR(properties.name), w, h, fr1/fr2, QSTRING_CSTR(sFormat));
													}
												}
											}
										}
										

										pType->Release();
									}
									reader->Release();
								}
								pSource->Release();
							}		
							properties.displayResolutions.sort(); 
							properties.framerates.sort();
							_deviceProperties.insert(dev, properties);							
						}		
						CoTaskMemFree(symlink);
					}
					CoTaskMemFree(name);
					devices[i]->Release();
				}

				CoTaskMemFree(devices);
			}
			attr->Release();		
		}
	}			
}

void QTCGrabber::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	_noSignalThresholdColor.red   = uint8_t(255*redSignalThreshold);
	_noSignalThresholdColor.green = uint8_t(255*greenSignalThreshold);
	_noSignalThresholdColor.blue  = uint8_t(255*blueSignalThreshold);
	_noSignalCounterThreshold     = qMax(1, noSignalCounterThreshold);

	Info(_log, "Signal threshold set to: {%d, %d, %d} and frames: %d", _noSignalThresholdColor.red, _noSignalThresholdColor.green, _noSignalThresholdColor.blue, _noSignalCounterThreshold );
}

void QTCGrabber::setSignalDetectionOffset(double horizontalMin, double verticalMin, double horizontalMax, double verticalMax)
{
	// rainbow 16 stripes 0.47 0.2 0.49 0.8
	// unicolor: 0.25 0.25 0.75 0.75

	_x_frac_min = horizontalMin;
	_y_frac_min = verticalMin;
	_x_frac_max = horizontalMax;
	_y_frac_max = verticalMax;

	Info(_log, "Signal detection area set to: %f,%f x %f,%f", _x_frac_min, _y_frac_min, _x_frac_max, _y_frac_max );
}

void	QTCGrabber::ResetCounter(uint64_t from)
{
	frameStat.frameBegin = from;
	frameStat.averageFrame = 0;
	frameStat.badFrame = 0;
	frameStat.goodFrame = 0;
	frameStat.segment = 0;
}

bool QTCGrabber::start()
{
	
	try
	{
		ResetCounter(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
		_QTCWorkerManager.Start();
				
		if (_QTCWorkerManager.workersCount<=1)
			Info(_log, "Multithreading for QTC is disabled. Available thread's count %d",_QTCWorkerManager.workersCount );
		else
			Info(_log, "Multithreading for QTC is enabled. Available thread's count %d",_QTCWorkerManager.workersCount );		
		
		if (init())
		{			
			start_capturing();
			Info(_log, "Started");
			return true;
		}
	}
	catch(std::exception& e)
	{
		Error(_log, "start failed (%s)", e.what());
	}

	return false;
}

void QTCGrabber::stop()
{
	if (_initialized)
	{								
		_QTCWorkerManager.Stop();				
		uninit_device();		
		_deviceProperties.clear();
		_initialized = false;
		Info(_log, "Stopped");
	}
}



bool QTCGrabber::init_device(QString deviceName, DevicePropertiesItem props)
{
	bool result = false;
	PixelFormat pixelformat;
	QString sFormat = identify_format(props.guid, pixelformat),error;
	QString guid = _deviceProperties[deviceName].name;
	HRESULT hr,hr1,hr2;
	
	Debug(_log,  "Init %s, %d x %d @ %d fps (%s) => %s", QSTRING_CSTR(deviceName), props.x, props.y, props.fps, QSTRING_CSTR(sFormat), QSTRING_CSTR(guid));
	
	IMFMediaSource* device;    
	IMFAttributes* pAttributes;
	IMFAttributes* attr;

	hr = MFCreateAttributes(&attr, 3);
	if (CHECK(hr))
	{

		hr = attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
		if (CHECK(hr))
		{
			WCHAR name[1024];			
			guid.toWCharArray(name);
			hr = attr->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, name);
			if (CHECK(hr) && _sourceReaderCB)
			{
				
					hr = MFCreateDeviceSource(attr, &device);
					if (!CHECK(hr))
					{
						device = NULL;
						error = QString("MFCreateDeviceSource %1").arg(hr);
					}
			
			}
			else
				error = QString("IMFAttributes_SetString_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK %1").arg(hr);
		}
		attr->Release();
	}
	else 
		error = QString("MFCreateAttributes_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE %1").arg(hr);
	
	
	if (device)
	{
		Debug(_log,  "Device opened");
		
		if (_brightness>0 || _contrast>0)					
		{
			
			IAMVideoProcAmp *pProcAmp = NULL;
			hr = device->QueryInterface(IID_PPV_ARGS(&pProcAmp));
			if (SUCCEEDED(hr))
			{
				long lMin, lMax, lStep, lDefault, lCaps, Val;
				
				if (_brightness>0)
				{
					hr = pProcAmp->GetRange(VideoProcAmp_Brightness, &lMin, &lMax, &lStep, &lDefault, &lCaps);

					if (SUCCEEDED(hr))
					{
						Debug(_log, "Brightness: min=%i, max=%i, default=%i", lMin, lMax, lDefault);
						
						hr = pProcAmp->Get(VideoProcAmp_Brightness, &Val,  &lCaps);
						if (SUCCEEDED(hr))
							Debug(_log, "Current brightness set to: %i",Val);
						
						hr = pProcAmp->Set(VideoProcAmp_Brightness, _brightness, VideoProcAmp_Flags_Manual);
						if (SUCCEEDED(hr))
						{
							Debug(_log, "Brightness set to: %i",_brightness);
						}
						else
							Error(_log, "Could not set brightness");
					}
					else
						Error(_log, "Brigthness is not supported by the grabber");
				}
				
				if (_contrast>0)
				{
					hr = pProcAmp->GetRange(VideoProcAmp_Contrast, &lMin, &lMax, &lStep, &lDefault, &lCaps);

					if (SUCCEEDED(hr))
					{
						Debug(_log, "Contrast: min=%i, max=%i, default=%i", lMin, lMax, lDefault);
						
						hr = pProcAmp->Get(VideoProcAmp_Contrast, &Val,  &lCaps);
						if (SUCCEEDED(hr))
							Debug(_log, "Current contrast set to: %i",Val);
					
						hr = pProcAmp->Set(VideoProcAmp_Contrast, _contrast, VideoProcAmp_Flags_Manual);
						if (SUCCEEDED(hr))
							Debug(_log, "Contrast set to: %i",_contrast);
						else
							Error(_log, "Could not set contrast");
					}
					else
						Error(_log, "Contrast is not supported by the grabber");
				}					
				
				pProcAmp->Release();
			}
						
		}
		
		hr1 = MFCreateAttributes(&pAttributes, 1);	
		
		if (CHECK(hr1))		
			hr2 = pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, (IMFSourceReaderCallback *)_sourceReaderCB);
		
	
		if (CHECK(hr1) && CHECK(hr2))					
			hr = MFCreateSourceReaderFromMediaSource(device, pAttributes, &READER);					
		else
			hr = E_INVALIDARG;
		
		if (CHECK(hr1))
			pAttributes->Release();
		
		device->Release();
		
        if (CHECK(hr))
		{			
            IMFMediaType* type;
			bool setStreamParamOK=false;
			
            hr = MFCreateMediaType(&type);
            if (CHECK(hr))
			{
				hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
				if (CHECK(hr))
				{
					hr = type->SetGUID(MF_MT_SUBTYPE, props.guid);
					if (CHECK(hr))
					{					
						hr = MFSetAttributeSize(type, MF_MT_FRAME_SIZE, props.x, props.y);				
						if (CHECK(hr))
						{
							hr = MFSetAttributeSize(type, MF_MT_FRAME_RATE, props.fps_a, props.fps_b);
							if (CHECK(hr))														
							{								
								hr = type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
								if (CHECK(hr))
								{
									MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
									
									hr = READER->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type);
									if (CHECK(hr))
									{
										setStreamParamOK = true;
									}
									else
										error = QString("SetCurrentMediaType %1").arg(hr);
								}
								else
										error = QString("SetUINT32_MF_MT_INTERLACE_MODE %1").arg(hr);
							}
							else
								error = QString("MFSetAttributeSize_MF_MT_FRAME_RATE %1").arg(hr);
						}
						else
							error = QString("SMFSetAttributeSize_MF_MT_FRAME_SIZE %1").arg(hr);
					}
					else
						error = QString("SetGUID_MF_MT_SUBTYPE %1").arg(hr);
				}
				else
					error = QString("SetGUID_MF_MT_MAJOR_TYPE %1").arg(hr);
				
				type->Release();
			}
			else
				error = QString("IMFAttributes_SetString %1").arg(hr);
					
				

            
			if (setStreamParamOK)
			{
				Debug(_log,  "+++++++++++++++++++++++++++++++++++++++++++++++++++++++");
				result = true;
			}
			else
				Error(_log,  "Could not stream set params (%s)", QSTRING_CSTR(error));
		}
		else
			Error(_log,  "MFCreateSourceReaderFromMediaSource (%i)", hr);
	}
	else
		Error(_log,  "Could not open device (%s)", QSTRING_CSTR(error));
	
	
	
	
		
	if (!result)
	{
		if (READER != NULL)
		{
			READER->Release();
			READER = NULL;
		}
	}	
	else
	{
		_pixelFormat = props.pf;
		_width = props.x;
		_height = props.y;
		
		
		switch (props.pf)
		{
			case PixelFormat::UYVY:
			{
				loadLutFile("yuv");						
				_frameByteSize = props.x * props.y * 2;				
				_lineLength = props.x * 2;
			}
			break;

			case PixelFormat::YUYV:
			{
				loadLutFile("yuv");		
				_frameByteSize = props.x * props.y * 2;
				_lineLength = props.x * 2;
			}
			break;

			case PixelFormat::RGB32:
			{
				loadLutFile("rgb");								
				_frameByteSize = props.x * props.y * 4;
				_lineLength = props.x * 3;
			}
			break;
			
			case PixelFormat::MJPEG:
			{				
				loadLutFile("rgb");						
				_lineLength = props.x * 3;

			}
			break;			
		}		
	}
	
	return result;
}

void QTCGrabber::uninit_device()
{
	if (READER != NULL)
	{
		READER->Release();
		READER = NULL;
	}
}

void QTCGrabber::start_capturing()
{	
	if (READER)
	{
		HRESULT hr = READER->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 
			0, NULL, NULL, NULL, NULL);
		if (!CHECK(hr))	
			Error(_log,  "ReadSample (%i)", hr);
	}	
}

void QTCGrabber::receive_image(const void *frameImageBuffer, int size, QString message)
{
	if (frameImageBuffer == NULL || size ==0)
		Error(_log,  "Received empty image frame: %s", QSTRING_CSTR(message));
	else
	{
		if (!message.isEmpty())
			Debug(_log,  "Received image frame: %s", QSTRING_CSTR(message));
		process_image(frameImageBuffer, size);
	}
	
	start_capturing();
}

bool QTCGrabber::process_image(const void *frameImageBuffer, int size)
{	
	bool frameSend = false;
	
	unsigned int processFrameIndex = _currentFrame++;
		
	// frame skipping
	if ( (processFrameIndex % _fpsSoftwareDecimation != 0) && (_fpsSoftwareDecimation > 1))
		return frameSend;
		
	// CEC detection
	if (_cecDetectionEnabled && _cecStandbyActivated)
		return frameSend;		

	// We do want a new frame...
	if (size < _frameByteSize && _pixelFormat != PixelFormat::MJPEG)
	{
		Error(_log, "Frame too small: %d != %d", size, _frameByteSize);
	}
	else
	{
		if (_QTCWorkerManager.isActive())
		{		
			// benchmark
			uint64_t currentTime=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			long	 diff = currentTime - frameStat.frameBegin;
			if ( diff >=1000*60)
			{
				int total = (frameStat.badFrame+frameStat.goodFrame);
				int av = (frameStat.goodFrame>0)?frameStat.averageFrame/frameStat.goodFrame:0;
				Debug(_log, "Video FPS: %.2f, av. delay: %dms, good: %d, bad: %d (%.2f,%d)", 
					total/60.0,
					av,
					frameStat.goodFrame,
					frameStat.badFrame,
					diff/1000.0,
					frameStat.segment);

				ResetCounter(currentTime);				
			}
			
			if (_QTCWorkerManager.workers == nullptr)
			{	
				_QTCWorkerManager.InitWorkers();
				Debug(_log, "Worker's thread count  = %d", _QTCWorkerManager.workersCount);	
				
				for (unsigned int i=0; i < _QTCWorkerManager.workersCount && _QTCWorkerManager.workers != nullptr; i++)
				{
					QTCWorker* _workerThread=_QTCWorkerManager.workers[i];					
					connect(_workerThread, SIGNAL(newFrameError(unsigned int, QString,unsigned int)), this , SLOT(newWorkerFrameError(unsigned int, QString,unsigned int)));
					connect(_workerThread, SIGNAL(newFrame(unsigned int, const Image<ColorRgb> &,unsigned int,quint64)), this , SLOT(newWorkerFrame(unsigned int, const Image<ColorRgb> &, unsigned int,quint64)));
				}
		    }	 
		    	
			
			
			for (unsigned int i=0;_QTCWorkerManager.isActive() && 
						i < _QTCWorkerManager.workersCount && 
						_QTCWorkerManager.workers != nullptr; i++)
			{													
				if ((_QTCWorkerManager.workers[i]->isFinished() || !_QTCWorkerManager.workers[i]->isRunning()))
					// aquire lock
					if ( _QTCWorkerManager.workers[i]->isBusy() == false)
					{		
						QTCWorker* _workerThread = _QTCWorkerManager.workers[i];	
						frameStat.segment|=(1<<(i));
						
						if ((_pixelFormat==PixelFormat::UYVY || _pixelFormat==PixelFormat::YUYV)  && !_lutBufferInit)
						{														
							if (lutBuffer == NULL)
								lutBuffer = (unsigned char *)malloc(LUT_FILE_SIZE + 1);
								
							for (int y = 0; y<256; y++)
								for (int u= 0; u<256; u++)
									for (int v = 0; v<256; v++)
									{
										uint32_t ind_lutd = LUT_INDEX(y, u, v);
										ColorSys::yuv2rgb(y, u, v, 
											lutBuffer[ind_lutd ], 
											lutBuffer[ind_lutd + 1], 
											lutBuffer[ind_lutd + 2]);
									}				
									
							_lutBufferInit = true;
										
							Error(_log,"You forgot to put lut_lin_tables.3d file in the Hyperion configuration folder. Internal LUT table for YUV conversion has been created instead.");
						}			
						
						_workerThread->setup(
							i, 							
							_videoMode,
							_pixelFormat,
							(uint8_t *)frameImageBuffer, size, _width, _height, _lineLength,				
							_subsamp, 				
							_cropLeft,  _cropTop, _cropBottom, _cropRight, 		
							processFrameIndex,currentTime,_hdrToneMappingEnabled,
							(_lutBufferInit)? lutBuffer: NULL);							
									
						if (_QTCWorkerManager.workersCount>1)					
							_QTCWorkerManager.workers[i]->start();
						else
							_QTCWorkerManager.workers[i]->startOnThisThread();
						//Debug(_log, "Frame index = %d => send to decode to the thread = %i", processFrameIndex,i);			
						frameSend = true;
						break;		
					}
			}						
		}		
	}

	return frameSend;
	
}

void QTCGrabber::newWorkerFrameError(unsigned int workerIndex, QString error, unsigned int sourceCount)
{
	
	frameStat.badFrame++;
	//Debug(_log, "Error occured while decoding mjpeg frame %d = %s", sourceCount, QSTRING_CSTR(error));	
	
	// get next frame	
	if (workerIndex >_QTCWorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);	
				
	if (workerIndex <=_QTCWorkerManager.workersCount)
		_QTCWorkerManager.workers[workerIndex]->noBusy();	
	
}


void QTCGrabber::newWorkerFrame(unsigned int workerIndex, const Image<ColorRgb>& image, unsigned int sourceCount, quint64 _frameBegin)
{
	
	frameStat.goodFrame++;
	frameStat.averageFrame += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()-_frameBegin;
	
	//Debug(_log, "Frame index = %d <= received from the thread and it's ready", sourceCount);	
		
	checkSignalDetectionEnabled(image);
	
	// get next frame	
	if (workerIndex >_QTCWorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);	
				
	if (workerIndex <=_QTCWorkerManager.workersCount)
		_QTCWorkerManager.workers[workerIndex]->noBusy();
	
}

void QTCGrabber::checkSignalDetectionEnabled(Image<ColorRgb> image)
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

void QTCGrabber::setSignalDetectionEnable(bool enable)
{
	if (_signalDetectionEnabled != enable)
	{
		_signalDetectionEnabled = enable;
		Info(_log, "Signal detection is now %s", enable ? "enabled" : "disabled");
	}
}

void QTCGrabber::setCecDetectionEnable(bool enable)
{
	if (_cecDetectionEnabled != enable)
	{
		_cecDetectionEnabled = enable;
		Info(_log, QString("CEC detection is now %1").arg(enable ? "enabled" : "disabled").toLocal8Bit());
	}
}

void QTCGrabber::setDeviceVideoStandard(QString device, VideoStandard videoStandard)
{
	if (_deviceName != device)
	{
		bool started = _initialized;
		uninit();
		
		_deviceName = device;

		if(started) start();
	}
}

bool QTCGrabber::setInput(int input)
{
	if(Grabber::setInput(input))
	{
		bool started = _initialized;
		uninit();
		if(started) start();
		return true;
	}
	return false;
}

bool QTCGrabber::setWidthHeight(int width, int height)
{
	if(Grabber::setWidthHeight(width,height))
	{
		Debug(_log,"setWidthHeight Restarting v4l2 grabber %i",_initialized);
		
		bool started = _initialized;
		uninit();
		if(started) start();
		return true;
	}
	return false;
}

bool QTCGrabber::setFramerate(int fps)
{
	if(Grabber::setFramerate(fps))
	{
		bool started = _initialized;
		uninit();
		if(started) start();
		return true;
	}
	return false;
}

QStringList QTCGrabber::getV4L2devices() const
{	
	QStringList result = QStringList();
	for (auto it = _deviceProperties.begin(); it != _deviceProperties.end(); ++it)
	{
		result << it.key();
	}
	return result;	
}

QString QTCGrabber::getV4L2deviceName(const QString& devicePath) const
{		
	return devicePath;
}

QMultiMap<QString, int> QTCGrabber::getV4L2deviceInputs(const QString& devicePath) const
{	
	return _deviceProperties.value(devicePath).inputs;
}

QStringList QTCGrabber::getResolutions(const QString& devicePath) const
{
	return _deviceProperties.value(devicePath).displayResolutions;
}

QStringList QTCGrabber::getFramerates(const QString& devicePath) const
{
	return _deviceProperties.value(devicePath).framerates;
}


void QTCGrabber::setEncoding(QString enc)
{
	PixelFormat _oldEnc = _enc;
	bool active = _QTCWorkerManager.isActive();
	
	_enc = parsePixelFormat(enc);	
	Debug(_log,"Force encoding (setEncoding): %s (%s)", QSTRING_CSTR(pixelFormatToString(_enc)), QSTRING_CSTR(pixelFormatToString(_oldEnc)));
			
	if (_oldEnc != _enc)
	{
		Debug(_log,"Restarting QTCGrabber grabber");
		
		bool started = _initialized;
		uninit();
		if(started) start();
	}
}

void QTCGrabber::setBrightnessContrast(uint8_t brightness, uint8_t contrast)
{
	if (_brightness != brightness || _contrast != contrast)
	{
		Debug(_log,"Set brightness to %i, contrast to %i",_brightness,_contrast);
		_brightness = brightness;
		_contrast = contrast;
		
		Debug(_log,"Restarting QTCGrabber grabber");
		
		bool started = _initialized;
		uninit();
		if(started) start();
	}
	else
		Debug(_log,"setBrightnessContrast nothing change");
}

