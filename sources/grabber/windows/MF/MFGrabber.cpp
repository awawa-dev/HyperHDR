/* MFGrabber.cpp
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
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <mferror.h>
#include <Guiddef.h>
#pragma push_macro("Info")
	#undef Info
	#include <strmif.h>
#pragma pop_macro("Info")


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

#include <grabber/windows/MF/MFGrabber.h>
#include <grabber/windows/MF/MFCallback.h>
#include <utils/GlobalSignals.h>

#include <turbojpeg.h>


#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "mf.lib")
#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "mfreadwrite.lib")


#define CHECK(hr) SUCCEEDED(hr)
#define CLEAR(x) memset(&(x), 0, sizeof(x))

// some stuff for HDR tone mapping
#define LUT_FILE_SIZE 50331648

typedef struct
{
	const GUID		format_id;
	const QString	format_name;
	PixelFormat		pixel;
} VideoFormat;

const static VideoFormat fmt_array[] =
{
	{ MFVideoFormat_RGB32,	"RGB32",  PixelFormat::XRGB },
	{ MFVideoFormat_ARGB32,	"ARGB32", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_RGB24,	"RGB24",  PixelFormat::RGB24 },
	{ MFVideoFormat_RGB555,	"RGB555", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_RGB565,	"RGB565", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_RGB8,	"RGB8", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_AI44,	"AI44", PixelFormat::NO_CHANGE},
	{ MFVideoFormat_AYUV,	"AYUV", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_YUY2,	"YUY2", PixelFormat::YUYV },
	{ MFVideoFormat_YVYU,	"YVYU", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_YVU9,	"YVU9", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_UYVY,	"UYVY", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_NV11,	"NV11", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_NV12,	"NV12", PixelFormat::NV12 },
	{ MFVideoFormat_YV12,	"YV12", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_I420,	"I420", PixelFormat::I420 },
	{ MFVideoFormat_IYUV,	"IYUV", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_Y210,	"Y210", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_Y216,	"Y216", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_Y410,	"Y410", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_Y416,	"Y416", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_Y41P,	"Y41P", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_Y41T,	"Y41T", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_Y42T,	"Y42T", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_P210,	"P210", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_P216,	"P216", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_P010,	"P010", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_P016,	"P016", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_v210,	"v210", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_v216,	"v216", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_v410,	"v410", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_MP43,	"MP43", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_MP4S,	"MP4S", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_M4S2,	"M4S2", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_MP4V,	"MP4V", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_WMV1,	"WMV1", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_WMV2,	"WMV2", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_WMV3,	"WMV3", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_WVC1,	"WVC1", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_MSS1,	"MSS1", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_MSS2,	"MSS2", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_MPG1,	"MPG1", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_DVSL,	"DVSL", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_DVSD,	"DVSD", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_DVHD,	"DVHD", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_DV25,	"DV25", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_DV50,	"DV50", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_DVH1,	"DVH1", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_DVC,	"DVC" , PixelFormat::NO_CHANGE },
	{ MFVideoFormat_H264,	"H264", PixelFormat::NO_CHANGE },
	{ MFVideoFormat_MJPG,	"MJPG", PixelFormat::MJPEG }
};

MFGrabber::MFGrabber(const QString& device, const QString& configurationPath)
	: Grabber(configurationPath, "MEDIA_FOUNDATION:" + device.left(14))
	, _sourceReader(NULL)
{
	// init MF
	_isMF = false;
	CoInitializeEx(0, COINIT_MULTITHREADED);

	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (!CHECK(hr))
	{
		CoUninitialize();
	}
	else
	{
		_sourceReaderCB = new MFCallback(this);
		_isMF = true;
	}

	// Refresh devices
	getMFdevices();
}

QString MFGrabber::GetSharedLut()
{
	return QCoreApplication::applicationDirPath();
}

void MFGrabber::loadLutFile(PixelFormat color)
{
	// load lut table
	QString fileName1 = QString("%1%2").arg(_configurationPath).arg("/lut_lin_tables.3d");
	QString fileName2 = QString("%1%2").arg(GetSharedLut()).arg("/lut_lin_tables.3d");

	Grabber::loadLutFile(_log, color, QList<QString>{fileName1, fileName2});
}

void MFGrabber::setHdrToneMappingEnabled(int mode)
{
	if (_hdrToneMappingEnabled != mode || _lut.data() == nullptr)
	{
		_hdrToneMappingEnabled = mode;
		if (_lut.data() != nullptr || !mode)
			Debug(_log, "setHdrToneMappingMode to: %s", (mode == 0) ? "Disabled" : ((mode == 1) ? "Fullscreen" : "Border mode"));
		else
			Warning(_log, "setHdrToneMappingMode to: enable, but the LUT file is currently unloaded");

		if (_MFWorkerManager.isActive())
		{
			Debug(_log, "setHdrToneMappingMode replacing LUT and restarting");
			_MFWorkerManager.Stop();
			if ((_actualVideoFormat == PixelFormat::YUYV) || (_actualVideoFormat == PixelFormat::I420) || (_actualVideoFormat == PixelFormat::NV12) || (_actualVideoFormat == PixelFormat::MJPEG))
				loadLutFile(PixelFormat::YUYV);
			else
				loadLutFile(PixelFormat::RGB24);
			_MFWorkerManager.Start();
		}
		emit SignalSetNewComponentStateToAllInstances(hyperhdr::Components::COMP_HDR, (mode != 0));
	}
	else
		Debug(_log, "setHdrToneMappingMode nothing changed: %s", (mode == 0) ? "Disabled" : ((mode == 1) ? "Fullscreen" : "Border mode"));
}

MFGrabber::~MFGrabber()
{
	uninit();

	// release MF capturing device
	if (_isMF)
	{
		if (_sourceReader != NULL)
		{
			_sourceReader->Release();
			_sourceReader = NULL;
		}

		if (_sourceReaderCB != NULL)
		{
			_sourceReaderCB->Release();
			_sourceReaderCB = NULL;
		}

		MFShutdown();
		CoUninitialize();
	}
}

void MFGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
		stop();
	}
}

bool MFGrabber::init()
{	
	if (!_initialized && _isMF)
	{
		QString foundDevice = "";
		int     foundIndex = -1;
		int     bestGuess = -1;
		int     bestGuessMinX = INT_MAX;
		int     bestGuessMinFPS = INT_MAX;
		bool    autoDiscovery = (QString::compare(_deviceName, Grabber::AUTO_SETTING, Qt::CaseInsensitive) == 0);

		enumerateMFdevices(true);



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

			if (bestGuess == -1 ||
				(val.x <= bestGuessMinX && val.x >= 640 &&
				 ((val.x > 800 && val.fps <= bestGuessMinFPS && val.fps >= 10) ||
				  (val.x <= 800 && val.fps > bestGuessMinFPS))))
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
			Info(_log, "Starting MF grabber. Selected: '%s' %d x %d @ %d fps %s", QSTRING_CSTR(foundDevice),
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


QString MFGrabber::identify_format(const GUID format, PixelFormat& pixelformat)
{
	for (int i = 0; i < sizeof(fmt_array) / sizeof(fmt_array[0]); i++)
		if (IsEqualGUID(format, fmt_array[i].format_id))
		{
			pixelformat = fmt_array[i].pixel;
			if (fmt_array[i].pixel == PixelFormat::NO_CHANGE)
				return fmt_array[i].format_name + " <unsupported>";
			else
				return fmt_array[i].format_name;
		}

	return "<unsupported>";
}

QString MFGrabber::FormatRes(int w, int h, QString format)
{
	QString ws = QString::number(w);
	QString hs = QString::number(h);

	while (ws.length() < 4)
		ws = " " + ws;
	while (hs.length() < 4)
		hs = " " + hs;

	return ws + "x" + hs + " " + format;
}

QString MFGrabber::FormatFrame(int fr)
{
	QString frame = QString::number(fr);

	while (frame.length() < 2)
		frame = " " + frame;

	return frame;
}

void MFGrabber::getMFdevices()
{
	enumerateMFdevices(false);
}

void MFGrabber::enumerateMFdevices(bool silent)
{

	QStringList result;
	HRESULT hr;
	UINT32 count;
	IMFActivate** devices;
	IMFAttributes* attr;

	if (!_isMF) {
		Error(_log, "enumerateMFdevices(): MF is not initialized");
		return;
	}
	else
		Info(_log, "Starting to enumerate video capture devices");

	_deviceProperties.clear();
	hr = MFCreateAttributes(&attr, 1);
	if (SUCCEEDED(hr))
	{
		hr = attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
		if (SUCCEEDED(hr))
		{
			hr = MFEnumDeviceSources(attr, &devices, &count);
			if (SUCCEEDED(hr))
			{
				Debug(_log, "Detected %u devices", count);

				for (UINT32 i = 0; i < count; i++)
				{
					UINT32 length;
					LPWSTR name;
					LPWSTR symlink;

					hr = devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &length);
					if (SUCCEEDED(hr))
					{
						hr = devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symlink, &length);
						if (SUCCEEDED(hr))
						{
							QString dev = QString::fromWCharArray(name);
							IMFMediaType* pType = NULL;
							IMFMediaSource* pSource = NULL;
							IAMVideoProcAmp* pProcAmp = NULL;

							DeviceProperties properties;
							properties.name = QString::fromWCharArray(symlink);

							Info(_log, "Found capture device: %s", QSTRING_CSTR(dev));

							hr = devices[i]->ActivateObject(IID_PPV_ARGS(&pSource));
							if (SUCCEEDED(hr))
							{
								IMFSourceReader* reader;

								// get device control capabilities							
								hr = pSource->QueryInterface(IID_PPV_ARGS(&pProcAmp));
								if (SUCCEEDED(hr))
								{
									long lStep, lCaps;

									hr = pProcAmp->GetRange(VideoProcAmp_Brightness, &properties.brightness.minVal, &properties.brightness.maxVal, &lStep, &properties.brightness.defVal, &lCaps);
									if (SUCCEEDED(hr))
									{
										properties.brightness.enabled = true;
										Debug(_log, "Device has 'brightness' control => min: %i, max: %i, default: %i", int(properties.brightness.minVal), int(properties.brightness.maxVal), int(properties.brightness.defVal));
									}

									hr = pProcAmp->GetRange(VideoProcAmp_Contrast, &properties.contrast.minVal, &properties.contrast.maxVal, &lStep, &properties.contrast.defVal, &lCaps);
									if (SUCCEEDED(hr))
									{
										properties.contrast.enabled = true;
										Debug(_log, "Device has 'contrast' control => min: %i, max: %i, default: %i", int(properties.contrast.minVal), int(properties.contrast.maxVal), int(properties.contrast.defVal));
									}

									hr = pProcAmp->GetRange(VideoProcAmp_Saturation, &properties.saturation.minVal, &properties.saturation.maxVal, &lStep, &properties.saturation.defVal, &lCaps);
									if (SUCCEEDED(hr))
									{
										properties.saturation.enabled = true;
										Debug(_log, "Device has 'saturation' control => min: %i, max: %i, default: %i", int(properties.saturation.minVal), int(properties.saturation.maxVal), int(properties.saturation.defVal));
									}

									hr = pProcAmp->GetRange(VideoProcAmp_Hue, &properties.hue.minVal, &properties.hue.maxVal, &lStep, &properties.hue.defVal, &lCaps);
									if (SUCCEEDED(hr))
									{
										properties.hue.enabled = true;
										Debug(_log, "Device has 'hue' control => min: %i, max: %i, default: %i", int(properties.hue.minVal), int(properties.hue.maxVal), int(properties.hue.defVal));
									}

									pProcAmp->Release();
								}

								hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &reader);
								if (SUCCEEDED(hr))
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
										if (SUCCEEDED(hr))
										{
											hr = pType->GetUINT64(MF_MT_FRAME_SIZE, &frame_size);
											if (SUCCEEDED(hr))
											{
												hr = pType->GetUINT64(MF_MT_FRAME_RATE, &frame_rate);
												if (SUCCEEDED(hr))
												{
													hr = pType->GetUINT32(MF_MT_INTERLACE_MODE, &interlace_mode);
													if (SUCCEEDED(hr) && interlace_mode == MFVideoInterlace_Progressive && frame_rate > 0)
													{
														PixelFormat pixelformat;
														DWORD w = frame_size >> 32;
														DWORD h = (DWORD)frame_size;
														DWORD fr1 = frame_rate >> 32;
														DWORD fr2 = (DWORD)frame_rate;
														QString sFormat = identify_format(format, pixelformat);
														if (!sFormat.contains("unsupported"))
														{

															int framerate = fr1 / fr2;
															QString sFrame = FormatFrame(framerate);
															QString resolution = FormatRes(w, h, sFormat);
															QString displayResolutions = FormatRes(w, h, "");

															if (properties.name.indexOf("usb#vid_534d&pid_2109", 0, Qt::CaseInsensitive) != -1 &&
																w == 1920 && h == 1080 && framerate == 60)
															{
																if (!silent)
																	Warning(_log, "BLACKLIST %s %d x %d @ %d fps %s (%s)", QSTRING_CSTR(dev), w, h, framerate, QSTRING_CSTR(sFormat), QSTRING_CSTR(pixelFormatToString(pixelformat)));
															}
															else
															{
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
																	Info(_log, "%s %d x %d @ %d fps %s (%s)", QSTRING_CSTR(dev), di.x, di.y, di.fps, QSTRING_CSTR(sFormat), QSTRING_CSTR(pixelFormatToString(di.pf)));
															}
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

bool MFGrabber::start()
{
	try
	{
		resetCounter(InternalClock::now());
		_MFWorkerManager.Start();

		if (_MFWorkerManager.workersCount <= 1)
			Info(_log, "Multithreading for MEDIA_FOUNDATION is disabled. Available thread's count %d", _MFWorkerManager.workersCount);
		else
			Info(_log, "Multithreading for MEDIA_FOUNDATION is enabled. Available thread's count %d", _MFWorkerManager.workersCount);

		if (init())
		{
			start_capturing();
			Info(_log, "Started");
			return true;
		}
	}
	catch (std::exception& e)
	{
		Error(_log, "start failed (%s)", e.what());
	}

	return false;
}

void MFGrabber::stop()
{
	if (_initialized)
	{
		_MFWorkerManager.Stop();
		uninit_device();
		_initialized = false;
		Info(_log, "Stopped");
	}
}

bool MFGrabber::init_device(QString selectedDeviceName, DevicePropertiesItem props)
{
	bool result = false;
	PixelFormat pixelformat;
	QString sFormat = identify_format(props.guid, pixelformat), error;
	DeviceProperties actDevice = _deviceProperties[selectedDeviceName];
	QString guid = _deviceProperties[selectedDeviceName].name;
	HRESULT hr, hr1, hr2;

	Info(_log, "Init %s, %d x %d @ %d fps (%s) => %s", QSTRING_CSTR(selectedDeviceName), props.x, props.y, props.fps, QSTRING_CSTR(sFormat), QSTRING_CSTR(guid));

	IMFMediaSource* device = NULL;
	IMFAttributes* pAttributes;
	IMFAttributes* attr;

	hr = MFCreateAttributes(&attr, 2);
	if (CHECK(hr))
	{

		hr = attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
		if (CHECK(hr))
		{
			int size = guid.length() + 1024;
			wchar_t* name = new wchar_t[size];
			memset(name, 0, size);
			guid.toWCharArray(name);

			hr = attr->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, (LPCWSTR)name);
			delete[] name;

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


	if (device != NULL)
	{
		IAMVideoProcAmp* pProcAmp = NULL;

		Info(_log, "Device opened successfully");

		hr = device->QueryInterface(IID_PPV_ARGS(&pProcAmp));
		if (SUCCEEDED(hr))
		{
			if (actDevice.brightness.enabled)
			{
				long selVal = (_brightness != 0) ? _brightness : actDevice.brightness.defVal;
				hr = pProcAmp->Set(VideoProcAmp_Brightness, selVal, VideoProcAmp_Flags_Manual);

				if (SUCCEEDED(hr))
					Info(_log, "Brightness set to: %i (%s)", selVal, (selVal == actDevice.brightness.defVal) ? "default" : "user");
				else
					Error(_log, "Could not set brightness to: %i", selVal);
			}

			if (actDevice.contrast.enabled)
			{
				long selVal = (_contrast != 0) ? _contrast : actDevice.contrast.defVal;
				hr = pProcAmp->Set(VideoProcAmp_Contrast, selVal, VideoProcAmp_Flags_Manual);

				if (SUCCEEDED(hr))
					Info(_log, "Contrast set to: %i (%s)", selVal, (selVal == actDevice.contrast.defVal) ? "default" : "user");
				else
					Error(_log, "Could not set contrast to: %i", selVal);
			}

			if (actDevice.saturation.enabled)
			{
				long selVal = (_saturation != 0) ? _saturation : actDevice.saturation.defVal;
				hr = pProcAmp->Set(VideoProcAmp_Saturation, selVal, VideoProcAmp_Flags_Manual);

				if (SUCCEEDED(hr))
					Info(_log, "Saturation set to: %i (%s)", selVal, (selVal == actDevice.saturation.defVal) ? "default" : "user");
				else
					Error(_log, "Could not set saturation to: %i", selVal);
			}

			if (actDevice.hue.enabled)
			{
				long selVal = (_hue != 0) ? _hue : actDevice.hue.defVal;
				hr = pProcAmp->Set(VideoProcAmp_Hue, selVal, VideoProcAmp_Flags_Manual);

				if (SUCCEEDED(hr))
					Info(_log, "Hue set to: %i (%s)", selVal, (selVal == actDevice.hue.defVal) ? "default" : "user");
				else
					Error(_log, "Could not set hue to: %i", selVal);
			}

			pProcAmp->Release();
		}

		hr1 = MFCreateAttributes(&pAttributes, 1);

		if (CHECK(hr1))
			hr2 = pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, (IMFSourceReaderCallback*)_sourceReaderCB);


		if (CHECK(hr1) && CHECK(hr2))
			hr = MFCreateSourceReaderFromMediaSource(device, pAttributes, &_sourceReader);
		else
			hr = E_INVALIDARG;

		if (CHECK(hr1))
			pAttributes->Release();

		device->Release();

		if (CHECK(hr))
		{
			IMFMediaType* type;
			bool setStreamParamOK = false;

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

									hr = _sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type);
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
				result = true;
			}
			else
				Error(_log, "Could not stream set params (%s)", QSTRING_CSTR(error));
		}
		else
			Error(_log, "MFCreateSourceReaderFromMediaSource (%i)", hr);
	}
	else
		Error(_log, "Could not open device (%s)", QSTRING_CSTR(error));





	if (!result)
	{
		if (_sourceReader != NULL)
		{
			_sourceReader->Release();
			_sourceReader = NULL;
		}
	}
	else
	{
		_actualVideoFormat = props.pf;
		_actualWidth = props.x;
		_actualHeight = props.y;
		_actualFPS = props.fps;
		_actualDeviceName = selectedDeviceName;

		Info(_log, "******************** SUCCESFULLY SET CAPTURE PARAMETERS: %i %i %s ********************", _actualWidth, _actualHeight, QSTRING_CSTR(pixelFormatToString(_actualVideoFormat)));


		switch (props.pf)
		{

		case PixelFormat::YUYV:
		{
			loadLutFile(PixelFormat::YUYV);
			_frameByteSize = props.x * props.y * 2;
			_lineLength = props.x * 2;
		}
		break;

		case PixelFormat::I420:
		{
			loadLutFile(PixelFormat::YUYV);
			_frameByteSize = (6 * props.x * props.y) / 4;
			_lineLength = props.x;
		}
		break;

		case PixelFormat::NV12:
		{
			loadLutFile(PixelFormat::YUYV);
			_frameByteSize = (6 * props.x * props.y) / 4;
			_lineLength = props.x;
		}
		break;

		case PixelFormat::RGB24:
		{
			loadLutFile(PixelFormat::RGB24);
			_frameByteSize = props.x * props.y * 3;
			_lineLength = props.x * 3;
		}
		break;

		case PixelFormat::XRGB:
		{
			loadLutFile(PixelFormat::RGB24);
			_frameByteSize = props.x * props.y * 4;
			_lineLength = props.x * 4;
		}
		break;

		case PixelFormat::MJPEG:
		{
			loadLutFile(PixelFormat::YUYV);
			_lineLength = props.x * 3;
		}
		break;
		}
	}

	return result;
}

void MFGrabber::uninit_device()
{
	_sourceReader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
	_sourceReaderCB->WaitToQuit();

	if (_sourceReader != NULL)
	{
		_sourceReader->Release();
		_sourceReader = NULL;
	}
}

void MFGrabber::start_capturing()
{
	if (_sourceReader)
	{
		HRESULT hr = _sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL);
		if (!CHECK(hr))
			Error(_log, "ReadSample (%i)", hr);
	}
}

void MFGrabber::receive_image(const void* frameImageBuffer, int size, QString message)
{
	if (frameImageBuffer == NULL || size == 0)
		Warning(_log, "Received empty image frame: %s", QSTRING_CSTR(message));
	else
	{
		if (!message.isEmpty())
			Debug(_log, "Received image frame: %s", QSTRING_CSTR(message));
		process_image(frameImageBuffer, size);
	}

	start_capturing();
}

bool MFGrabber::process_image(const void* frameImageBuffer, int size)
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
		if (_MFWorkerManager.isActive())
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

			if (_MFWorkerManager.workers == nullptr)
			{
				_MFWorkerManager.InitWorkers();
				Debug(_log, "Worker's thread count  = %d", _MFWorkerManager.workersCount);

				for (unsigned int i = 0; i < _MFWorkerManager.workersCount && _MFWorkerManager.workers != nullptr; i++)
				{
					MFWorker* _workerThread = _MFWorkerManager.workers[i];
					connect(_workerThread, &MFWorker::SignalNewFrameError, this, &MFGrabber::newWorkerFrameErrorHandler);
					connect(_workerThread, &MFWorker::SignalNewFrame, this, &MFGrabber::newWorkerFrameHandler);
				}
			}

			for (unsigned int i = 0; _MFWorkerManager.isActive() && i < _MFWorkerManager.workersCount && _MFWorkerManager.workers != nullptr; i++)
			{
				if (_MFWorkerManager.workers[i]->isFinished() || !_MFWorkerManager.workers[i]->isRunning())
				{
					if (_MFWorkerManager.workers[i]->isBusy() == false)
					{
						MFWorker* _workerThread = _MFWorkerManager.workers[i];

						if ((_actualVideoFormat == PixelFormat::YUYV || _actualVideoFormat == PixelFormat::I420 ||
							_actualVideoFormat == PixelFormat::NV12) && !_lutBufferInit)
						{
							loadLutFile();
						}

						bool directAccess = !(_signalAutoDetectionEnabled || _signalDetectionEnabled || isCalibrating() || (_benchmarkStatus >= 0));
						_workerThread->setup(
							i,
							_actualVideoFormat,
							(uint8_t*)frameImageBuffer, size, _actualWidth, _actualHeight, _lineLength,
							_cropLeft, _cropTop, _cropBottom, _cropRight,
							processFrameIndex, InternalClock::nowPrecise(), _hdrToneMappingEnabled,
							(_lutBufferInit) ? _lut.data() : nullptr, _qframe, directAccess, _deviceName);

						if (_MFWorkerManager.workersCount > 1)
							_MFWorkerManager.workers[i]->start();
						else
							_MFWorkerManager.workers[i]->startOnThisThread();

						frameSend = true;
						break;
					}
				}
			}
		}
	}

	return frameSend;
}

void MFGrabber::newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount)
{

	frameStat.badFrame++;
	if (error.indexOf(QString(UNSUPPORTED_DECODER)) == 0)
	{
		Error(_log, "Unsupported MJPEG/YUV format. Please contact HyperHDR developers! (info: %s)", QSTRING_CSTR(error));
	}
	//Debug(_log, "Error occured while decoding mjpeg frame %d = %s", sourceCount, QSTRING_CSTR(error));	

	// get next frame	
	if (workerIndex > _MFWorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);

	if (workerIndex <= _MFWorkerManager.workersCount)
		_MFWorkerManager.workers[workerIndex]->noBusy();

}


void MFGrabber::newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin)
{
	handleNewFrame(workerIndex, image, sourceCount, _frameBegin);

	// get next frame	
	if (workerIndex > _MFWorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);

	if (workerIndex <= _MFWorkerManager.workersCount)
		_MFWorkerManager.workers[workerIndex]->noBusy();
}


