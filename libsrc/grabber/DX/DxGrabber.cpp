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

#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/HyperHdrIManager.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include <grabber/DxGrabber.h>
#include <utils/ColorSys.h>


#pragma comment (lib, "ole32.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

#define CHECK(hr) SUCCEEDED(hr)
#define CLEAR(x) memset(&(x), 0, sizeof(x))

template <class T> void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

DxGrabber::DxGrabber(const QString& device, const QString& configurationPath)
	: Grabber("DX11_SYSTEM:" + device.left(14))
	, _configurationPath(configurationPath)
	, _semaphore(1)
	, _warningCounter(5)

	, dxRestartNow(false)
	, d3dCache(false)
	, d3dDevice(nullptr)
	, d3dContext(nullptr)
	, sourceTexture(nullptr)
	, d3dDuplicate(nullptr)
{
	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &DxGrabber::grabFrame);

	// Refresh devices
	getDevices();
}

QString DxGrabber::GetSharedLut()
{
	return QCoreApplication::applicationDirPath();
}

void DxGrabber::loadLutFile(PixelFormat color)
{	
	// load lut table
	QString fileName1 = QString("%1%2").arg(_configurationPath).arg("/lut_lin_tables.3d");
	QString fileName2 = QString("%1%2").arg(GetSharedLut()).arg("/lut_lin_tables.3d");

	Grabber::loadLutFile(color, QList<QString>{fileName1, fileName2});
}

void DxGrabber::setHdrToneMappingEnabled(int mode)
{
	if (_hdrToneMappingEnabled != mode || _lutBuffer == NULL)
	{
		_hdrToneMappingEnabled = mode;
		if (_lutBuffer != NULL || !mode)
			Debug(_log, "setHdrToneMappingMode to: %s", (mode == 0) ? "Disabled" : ((mode == 1) ? "Fullscreen" : "Border mode"));
		else
			Warning(_log, "setHdrToneMappingMode to: enable, but the LUT file is currently unloaded");

		
		loadLutFile(PixelFormat::RGB24);		
	}
	else
		Debug(_log, "setHdrToneMappingMode nothing changed: %s", (mode == 0) ? "Disabled" : ((mode == 1) ? "Fullscreen" : "Border mode"));
}

DxGrabber::~DxGrabber()
{
	uninit();

	// destroy core elements from contructor
}

void DxGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{		
		stop();
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
	}

	d3dCache = false;
	SafeRelease(&sourceTexture);
	SafeRelease(&d3dDuplicate);
	SafeRelease(&d3dContext);
	SafeRelease(&d3dDevice);

	_warningCounter = 5;
	_initialized = false;

	if (dxRestartNow)
	{
		dxRestartNow = false;
		QTimer::singleShot(1000, this, &DxGrabber::restart);
	}
}

void DxGrabber::restart()
{	
	if (!_initialized)
	{
		start();
	}
}

bool DxGrabber::init()
{
	Debug(_log, "init");


	if (!_initialized)
	{
		QString foundDevice = "";
		bool    autoDiscovery = (QString::compare(_deviceName, Grabber::AUTO_SETTING, Qt::CaseInsensitive) == 0);

		enumerateDevices(true);


		if (!autoDiscovery && !_deviceProperties.contains(_deviceName))
		{
			Debug(_log, "Device %s is not available. Changing to auto.", QSTRING_CSTR(_deviceName));
			autoDiscovery = true;
		}

		if (autoDiscovery)
		{
			Debug(_log, "Forcing auto discovery device");
			if (!_deviceProperties.isEmpty())
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

		
		Info(_log, "*************************************************************************************************");
		Info(_log, "Starting DX grabber. Selected: '%s' max width: %d (%d) @ %d fps", QSTRING_CSTR(foundDevice), _width, _height, _fps);
		Info(_log, "*************************************************************************************************");

		if (init_device(foundDevice))
			_initialized = true;		
	}

	return _initialized;
}


void DxGrabber::getDevices()
{
	enumerateDevices(false);
}

void DxGrabber::enumerateDevices(bool silent)
{	
	IDXGIFactory1* pFactory = NULL;

	_deviceProperties.clear();

	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&pFactory)))
	{
		Error(_log, "Can't enumerate devices");
		return;
	}
	
	IDXGIAdapter1 * pAdapter; 	
	for(UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; i++)
	{
		DeviceProperties properties;
		DXGI_ADAPTER_DESC1 pDesc;

		pAdapter->GetDesc1(&pDesc);

		IDXGIOutput* pOutput;
		for (UINT j = 0; pAdapter->EnumOutputs(j, &pOutput) != DXGI_ERROR_NOT_FOUND; j++)
		{
			DXGI_OUTPUT_DESC oDesc;
			pOutput->GetDesc(&oDesc);

			QString dev = QString::fromWCharArray(oDesc.DeviceName) + "|" + QString::fromWCharArray(pDesc.Description);
			_deviceProperties.insert(dev, properties);

			SafeRelease(&pOutput);
		}		
		SafeRelease(&pAdapter);
	} 
	SafeRelease(&pFactory);
}

bool DxGrabber::start()
{
	try
	{		
		if (init())
		{
			_timer.setInterval(1000/_fps);
			_timer.start();
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

void DxGrabber::stop()
{
	if (_initialized)
	{
		_semaphore.acquire();
		_timer.stop();
		_semaphore.release();
		Info(_log, "Stopped");
	}
}

bool DxGrabber::init_device(QString selectedDeviceName)
{
	bool result = false, exitNow = false;
	IDXGIFactory1* pFactory = NULL;	

	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&pFactory)))
	{
		return result;
	}

	IDXGIAdapter1* pAdapter;
	for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND && !exitNow; i++)
	{
		DeviceProperties properties;
		DXGI_ADAPTER_DESC1 pDesc;

		pAdapter->GetDesc1(&pDesc);

		IDXGIOutput* pOutput;
		for (UINT j = 0; pAdapter->EnumOutputs(j, &pOutput) != DXGI_ERROR_NOT_FOUND && !exitNow; j++)
		{
			DXGI_OUTPUT_DESC oDesc;
			pOutput->GetDesc(&oDesc);

			exitNow = (QString::fromWCharArray(oDesc.DeviceName) + "|" + QString::fromWCharArray(pDesc.Description)) == selectedDeviceName;

			if (exitNow)
			{
				IDXGIOutput1* pOutput1 = nullptr;

				if (CHECK(pOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&pOutput1))))
				{
					HRESULT findDriver = E_FAIL;
					DXGI_OUTDUPL_DESC duplicateDesc;
					D3D_FEATURE_LEVEL featureLevel;
					std::vector<D3D_DRIVER_TYPE> driverTypes{
						D3D_DRIVER_TYPE_HARDWARE,
						D3D_DRIVER_TYPE_WARP,
						D3D_DRIVER_TYPE_REFERENCE,
						D3D_DRIVER_TYPE_UNKNOWN
					};

					CLEAR(duplicateDesc);
					CLEAR(featureLevel);

					for (auto& driverType : driverTypes)
					{
						findDriver = D3D11CreateDevice(pAdapter, driverType,
							nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
							D3D11_SDK_VERSION, &d3dDevice, &featureLevel, &d3dContext);
						if (SUCCEEDED(findDriver))
						{
							switch (driverType)
							{
								case D3D_DRIVER_TYPE_HARDWARE: Info(_log, "Selected D3D_DRIVER_TYPE_HARDWARE"); break;
								case D3D_DRIVER_TYPE_WARP: Info(_log, "Selected D3D_DRIVER_TYPE_WARP"); break;
								case D3D_DRIVER_TYPE_REFERENCE: Info(_log, "Selected D3D_DRIVER_TYPE_REFERENCE"); break;
								case D3D_DRIVER_TYPE_UNKNOWN: Info(_log, "Selected D3D_DRIVER_TYPE_UNKNOWN"); break;
							}
							break;
						}
					}

					if (CHECK(findDriver) && d3dDevice != nullptr)
					{
						HRESULT status = pOutput1->DuplicateOutput(d3dDevice, &d3dDuplicate);
						if (CHECK(status))
						{
							d3dDuplicate->GetDesc(&duplicateDesc);

							D3D11_TEXTURE2D_DESC sourceTextureDesc;
							sourceTextureDesc.Width = duplicateDesc.ModeDesc.Width;
							sourceTextureDesc.Height = duplicateDesc.ModeDesc.Height;
							sourceTextureDesc.ArraySize = 1;
							sourceTextureDesc.MipLevels = 1;
							sourceTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
							sourceTextureDesc.SampleDesc.Count = 1;
							sourceTextureDesc.SampleDesc.Quality = 0;
							sourceTextureDesc.Usage = D3D11_USAGE_STAGING;
							sourceTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
							sourceTextureDesc.MiscFlags = 0;
							sourceTextureDesc.BindFlags = 0;

							if (CHECK(d3dDevice->CreateTexture2D(&sourceTextureDesc, NULL, &sourceTexture)))
							{
								_actualVideoFormat = PixelFormat::XRGB;
								_actualWidth = duplicateDesc.ModeDesc.Width;
								_actualHeight = duplicateDesc.ModeDesc.Height;

								loadLutFile(PixelFormat::RGB24);
								_frameByteSize = _actualWidth * _actualHeight * 4;
								_lineLength = _actualWidth * 4;

								Info(_log, "Device initialized");
								result = true;
							}
							else
							{
								Error(_log, "CreateTexture2D failed");
								uninit();
							}
						}
						else
						{							
							uninit();

							if (status == E_ACCESSDENIED)
							{
								Error(_log, "The device is temporarily unavailable. Will try in 5 seconds again.");
								QTimer::singleShot(5000, this, &DxGrabber::restart);
							}
							else
								Error(_log, "Could not create a mirror for d3d device (busy resources?). Code: %i", status);
						}
					}
					else
						Error(_log, "Could not found any d3d device");
					SafeRelease(&pOutput1);
				}
				else
					Error(_log, "Cast to IDXGIOutput1 failed");
			}
			SafeRelease(&pOutput);
			exitNow = true;
		}
		SafeRelease(&pAdapter);
	}

	SafeRelease(&pFactory);

	return result;
}

void DxGrabber::grabFrame()
{
	DXGI_OUTDUPL_FRAME_INFO infoFrame;
	DXGI_MAPPED_RECT mappedRect;
	IDXGIResource* resourceDesktop = nullptr;

	CLEAR(mappedRect);
	
	if (_semaphore.tryAcquire())
	{
		auto status = d3dDuplicate->AcquireNextFrame(0, &infoFrame, &resourceDesktop);

		if (CHECK(status))
		{
			status = d3dDuplicate->MapDesktopSurface(&mappedRect);
			if (CHECK(status)) {
				processSystemFrameBGRA((uint8_t*)mappedRect.pBits);
				d3dDuplicate->UnMapDesktopSurface();
			}
			else if (status == DXGI_ERROR_UNSUPPORTED)
			{
				D3D11_MAPPED_SUBRESOURCE internalMap;
				ID3D11Texture2D* texDesktop = nullptr;

				CLEAR(internalMap);

				status = resourceDesktop->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texDesktop);
				if (CHECK(status) && texDesktop != nullptr)
				{
					d3dContext->CopyResource(sourceTexture, texDesktop);


					if (CHECK(d3dContext->Map(sourceTexture, 0, D3D11_MAP_READ, 0, &internalMap)))
					{
						uint8_t* data = (unsigned char*)internalMap.pData;

						d3dCache = true;
						processSystemFrameBGRA((uint8_t*)internalMap.pData, (int)internalMap.RowPitch);
						d3dContext->Unmap(sourceTexture, 0);
					}

					SafeRelease(&texDesktop);
				}
				else if (_warningCounter > 0)
				{
					Error(_log, "ResourceDesktop->QueryInterface failed. Reason: %i", status);
					_warningCounter--;
				}
			}
			else if (_warningCounter > 0)
			{
				Error(_log, "MapDesktopSurface failed. Reason: %i", status);
				_warningCounter--;
			}
		}
		else if (status == DXGI_ERROR_WAIT_TIMEOUT && d3dCache)
		{
			D3D11_MAPPED_SUBRESOURCE internalMap;

			CLEAR(internalMap);

			if (_warningCounter > 0)
			{
				Debug(_log, "AcquireNextFrame didn't return the frame. Just warning: the screen has not changed?");
				_warningCounter--;
			}

			if (CHECK(d3dContext->Map(sourceTexture, 0, D3D11_MAP_READ, 0, &internalMap)))
			{
				uint8_t* data = (unsigned char*)internalMap.pData;

				processSystemFrameBGRA((uint8_t*)internalMap.pData);
				d3dContext->Unmap(sourceTexture, 0);
			}
		}
		else if (status == DXGI_ERROR_ACCESS_LOST)
		{
			Error(_log, "Lost DirectX capture context. Stopping.");
			dxRestartNow = true;
		}
		else if (_warningCounter > 0)
		{
			Error(_log, "AcquireNextFrame failed. Reason: %i", status);
			_warningCounter--;
		}

		SafeRelease(&resourceDesktop);

		d3dDuplicate->ReleaseFrame();

		_semaphore.release();

		if (dxRestartNow)
		{
			uninit();
		}
	}
}


void DxGrabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;
}
