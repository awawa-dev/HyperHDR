/* DxGrabber.cpp
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
#define NOMINMAX

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
#include <DirectXMath.h>

#include <base/HyperHdrInstance.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include <grabber/windows/DX/DxGrabber.h>
#include <grabber/windows/DX/VertexShaderHyperHDR.h>
#include <grabber/windows/DX/PixelShaderHyperHDR.h>

#pragma comment (lib, "ole32.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

#define CHECK(hr) SUCCEEDED(hr)
#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define MAX_WARNINGS 6

template <class T> void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

DxGrabber::DxGrabber(const QString& device, const QString& configurationPath)
	: Grabber(configurationPath, "DX11_SYSTEM:" + device.left(14))
	, _configurationPath(configurationPath)
	, _timer(new QTimer(this))
	, _retryTimer(new QTimer(this))
	, _warningCounter(MAX_WARNINGS)
	, _actualDivide(-1)
	, _wideGamut(false)
	

	, _dxRestartNow(false)
	, _d3dDevice(nullptr)
	, _d3dContext(nullptr)
	, _d3dBuffer(nullptr)
	, _d3dSampler(nullptr)
	, _d3dVertexLayout(nullptr)
	, _d3dVertexShader(nullptr)
	, _d3dPixelShader(nullptr)
	, _d3dSourceTexture(nullptr)
	, _d3dConvertTexture(nullptr)
	, _d3dConvertTextureView(nullptr)
	, _d3dRenderTargetView(nullptr)
	, _d3dDuplicate(nullptr)
{
	_timer->setTimerType(Qt::PreciseTimer);
	connect(_timer, &QTimer::timeout, this, &DxGrabber::grabFrame);

	_retryTimer->setSingleShot(true);
	connect(_retryTimer, &QTimer::timeout, this, &DxGrabber::restart);

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

	Grabber::loadLutFile(_log, color, QList<QString>{fileName1, fileName2});
}

void DxGrabber::setHdrToneMappingEnabled(int mode)
{
	if (_hdrToneMappingEnabled != mode || _lut.data() == nullptr)
	{
		_hdrToneMappingEnabled = mode;
		if (_lut.data() != nullptr || !mode)
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
}

void DxGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{
		disconnect(this, &Grabber::SignalNewCapturedFrame, this, &DxGrabber::cacheHandler);
		_cacheImage = Image<ColorRgb>();

		stop();
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
	}

	SafeRelease(&_d3dRenderTargetView);
	SafeRelease(&_d3dSourceTexture);
	SafeRelease(&_d3dConvertTextureView);
	SafeRelease(&_d3dConvertTexture);
	SafeRelease(&_d3dVertexShader);
	SafeRelease(&_d3dVertexLayout);
	SafeRelease(&_d3dPixelShader);
	SafeRelease(&_d3dSampler);
	SafeRelease(&_d3dBuffer);
	SafeRelease(&_d3dDuplicate);
	SafeRelease(&_d3dContext);
	SafeRelease(&_d3dDevice);

	_warningCounter = MAX_WARNINGS;
	_actualDivide = -1;
	_initialized = false;

	if (_dxRestartNow)
	{
		_dxRestartNow = false;
		_retryTimer->setInterval(1000);
		_retryTimer->start();
	}
}

void DxGrabber::newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin)
{
};

void DxGrabber::newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount)
{
};

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

		if (initDirectX(foundDevice))
		{
			connect(this, &Grabber::SignalNewCapturedFrame, this, &DxGrabber::cacheHandler, Qt::UniqueConnection);
			_initialized = true;
		}
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

	IDXGIAdapter1* pAdapter;
	for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; i++)
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
			_timer->setInterval(1000 / _fps);
			_timer->start();
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
		_timer->stop();
		Info(_log, "Stopped");
	}

	_retryTimer->stop();
}

bool DxGrabber::initDirectX(QString selectedDeviceName)
{
	bool result = false, exitNow = false;
	IDXGIFactory2* pFactory = NULL;

	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&pFactory)))
	{
		Error(_log, "Could not create IDXGIFactory2");
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
				IDXGIOutput6* pOutput6 = nullptr;

				if (CHECK(pOutput->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void**>(&pOutput6))))
				{
					HRESULT findDriver = E_FAIL;
					D3D_FEATURE_LEVEL featureLevel;
					std::vector<D3D_DRIVER_TYPE> driverTypes{
						D3D_DRIVER_TYPE_HARDWARE,
						D3D_DRIVER_TYPE_WARP,
						D3D_DRIVER_TYPE_REFERENCE,
						D3D_DRIVER_TYPE_UNKNOWN
					};
					
					CLEAR(featureLevel);

					for (auto& driverType : driverTypes)
					{
						findDriver = D3D11CreateDevice(pAdapter, driverType,
							nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
							D3D11_SDK_VERSION, &_d3dDevice, &featureLevel, &_d3dContext);

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
					
					if (CHECK(findDriver) && _d3dDevice != nullptr)
					{
						HRESULT status = E_FAIL;
						DXGI_OUTPUT_DESC1 descGamut;

						pOutput6->GetDesc1(&descGamut);

						_wideGamut = descGamut.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
						Info(_log, "Gamut: %s, min nits: %0.2f, max nits: %0.2f, max frame nits: %0.2f, white point: [%0.2f, %0.2f]",
									(_wideGamut) ? "HDR" : "SDR", descGamut.MinLuminance, descGamut.MaxLuminance, descGamut.MaxFullFrameLuminance,
									descGamut.WhitePoint[0], descGamut.WhitePoint[1]);

						if (_wideGamut && _targetMonitorNits == 0)
						{
							Warning(_log, "Target SDR brightness is set to %i nits. Disabling wide gamut.", _targetMonitorNits);
							_wideGamut = false;
						}


						if (_hardware && _wideGamut)
						{
							Info(_log, "Using wide gamut for HDR. Target SDR brightness: %i nits", _targetMonitorNits);

							std::vector<DXGI_FORMAT> wideFormat({ DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R10G10B10A2_UNORM });
							status = pOutput6->DuplicateOutput1(_d3dDevice, 0, static_cast<UINT>(wideFormat.size()), wideFormat.data(), &_d3dDuplicate);

							if (!CHECK(status))
							{
								Warning(_log, "No support for DXGI_FORMAT_R16G16B16A16_FLOAT/DXGI_FORMAT_R10G10B10A2_UNORM. Fallback to BGRA");
								_wideGamut = false;
							}
						}

						if (!CHECK(status))
						{
							Info(_log, "Using BGRA format");

							DXGI_FORMAT rgbFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
							status = pOutput6->DuplicateOutput1(_d3dDevice, 0, 1, &rgbFormat, &_d3dDuplicate);
						}

						if (CHECK(status))
						{
							CLEAR(_surfaceProperties);
							_d3dDuplicate->GetDesc(&_surfaceProperties);

							Info(_log, "Surface format: %i", _surfaceProperties.ModeDesc.Format);

							int targetSizeX = _surfaceProperties.ModeDesc.Width, targetSizeY = _surfaceProperties.ModeDesc.Height;

							if (_hardware)
							{								
								_actualWidth = targetSizeX;
								_actualHeight = targetSizeY;

								if (!_wideGamut)
								{
									int maxSize = std::max((_actualWidth - _cropLeft - _cropRight), (_actualHeight - _cropTop - _cropBottom));

									_actualDivide = 0;
									while (maxSize > _width)
									{
										_actualDivide++;
										maxSize >>= 1;
									}

									targetSizeX = _surfaceProperties.ModeDesc.Width >> _actualDivide;
									targetSizeY = _surfaceProperties.ModeDesc.Height >> _actualDivide;
								}
								else
								{
									_actualDivide = -1;
									getTargetSystemFrameDimension(targetSizeX, targetSizeY);
								}
							}

							D3D11_TEXTURE2D_DESC sourceTextureDesc{};
							CLEAR(sourceTextureDesc);
							sourceTextureDesc.Width = targetSizeX;
							sourceTextureDesc.Height = targetSizeY;
							sourceTextureDesc.ArraySize = 1;
							sourceTextureDesc.MipLevels = 1;
							sourceTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
							sourceTextureDesc.SampleDesc.Count = 1;
							sourceTextureDesc.SampleDesc.Quality = 0;
							sourceTextureDesc.Usage = D3D11_USAGE_STAGING;
							sourceTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
							sourceTextureDesc.MiscFlags = 0;
							sourceTextureDesc.BindFlags = 0;
							if (CHECK(_d3dDevice->CreateTexture2D(&sourceTextureDesc, NULL, &_d3dSourceTexture)))
							{
								_actualVideoFormat = PixelFormat::XRGB;
								_actualWidth = sourceTextureDesc.Width;
								_actualHeight = sourceTextureDesc.Height;

								loadLutFile(PixelFormat::RGB24);
								_frameByteSize = _actualWidth * _actualHeight * 4;
								_lineLength = _actualWidth * 4;

								if (_hardware)
								{
									result = initShaders();
									if (result)
									{										
										Info(_log, "The DX11 device has been initialized. Hardware acceleration is enabled");										
									}
									else
									{
										Error(_log, "CreateShaders failed");
										uninit();
									}	
								}
								else
								{
									result = true;
									Info(_log, "The DX11 device has been initialized. Hardware acceleration is disabled");
								}
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
								_retryTimer->setInterval(5000);
								_retryTimer->start();
							}
							else
								Error(_log, "Could not create a mirror for d3d device (busy resources?). Code: %i", status);
						}
					}
					else
						Error(_log, "Could not found any d3d device");

					SafeRelease(&pOutput6);
				}
				else
					Error(_log, "Cast to IDXGIOutput1 failed");
			}
			SafeRelease(&pOutput);
		}
		SafeRelease(&pAdapter);
	}

	SafeRelease(&pFactory);

	return result;
}

bool DxGrabber::initShaders()
{
	HRESULT status;

	std::unique_ptr<CD3D11_TEXTURE2D_DESC> descConvert;

	if (_actualDivide < 0)
		descConvert = std::make_unique<CD3D11_TEXTURE2D_DESC>(
						DXGI_FORMAT_B8G8R8A8_UNORM,
						_actualWidth, _actualHeight,
						1,
						1,
						D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
						D3D11_USAGE_DEFAULT,
						0,
						1);
	else
		descConvert = std::make_unique<CD3D11_TEXTURE2D_DESC>(
						DXGI_FORMAT_B8G8R8A8_UNORM,
						_surfaceProperties.ModeDesc.Width, _surfaceProperties.ModeDesc.Height,
						1,
						_actualDivide + 1,
						D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
						D3D11_USAGE_DEFAULT,
						0,
						1,
						0,
						D3D11_RESOURCE_MISC_GENERATE_MIPS);

	status = _d3dDevice->CreateTexture2D(descConvert.get(), nullptr, &_d3dConvertTexture);
	if (CHECK(status))
	{
		if (_actualDivide < 0)
		{
			CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(
				D3D11_RTV_DIMENSION_TEXTURE2D,
				DXGI_FORMAT_B8G8R8A8_UNORM);

			status = _d3dDevice->CreateRenderTargetView(_d3dConvertTexture, &rtvDesc, &_d3dRenderTargetView);
			if (CHECK(status))
			{
				_d3dContext->OMSetRenderTargets(1, &_d3dRenderTargetView, NULL);
			}
			else
				Error(_log, "CreateRenderTargetView failed. Reason: %x", status);
		}
		else
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc = {};

			CLEAR(shaderDesc);
			shaderDesc.Format = descConvert->Format;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderDesc.Texture2D.MipLevels = descConvert->MipLevels;;
			
			status = _d3dDevice->CreateShaderResourceView(_d3dConvertTexture, &shaderDesc, &_d3dConvertTextureView);
			return CHECK(status);
		}
	}
	else
		Error(_log, "CreateConvertTexture2D failed. Reason: %x", status);

	if (CHECK(status))
	{
		status = _d3dDevice->CreateVertexShader(g_VertexShaderHyperHDR, sizeof(g_VertexShaderHyperHDR), nullptr, &_d3dVertexShader);
		if (CHECK(status))
		{
			_d3dContext->VSSetShader(_d3dVertexShader, NULL, 0);
			_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		}
		else
			Error(_log, "Could not create vertex shaders. Reason: %x", status);
	}

	if (CHECK(status))
	{
		status = _d3dDevice->CreatePixelShader(g_PixelShaderHyperHDR, sizeof(g_PixelShaderHyperHDR), nullptr, &_d3dPixelShader);
		if (CHECK(status))
		{
			_d3dContext->PSSetShader(_d3dPixelShader, NULL, 0);
		}
		else
			Error(_log, "Could not create pixel shaders. Reason: %x", status);
	}

	if (CHECK(status))
	{
		D3D11_INPUT_ELEMENT_DESC layout[] = { {"SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}, };
		UINT numElements = ARRAYSIZE(layout);

		status = _d3dDevice->CreateInputLayout(layout, numElements, g_VertexShaderHyperHDR, sizeof(g_VertexShaderHyperHDR), &_d3dVertexLayout);
		if (CHECK(status))
			_d3dContext->IASetInputLayout(_d3dVertexLayout);
		else
			Error(_log, "Could not create vertex layout. Reason: %x", status);
	}

	if (CHECK(status))
	{
		D3D11_SAMPLER_DESC sDesc{};
		CLEAR(sDesc);

		sDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sDesc.MinLOD = 0;
		sDesc.MaxLOD = D3D11_FLOAT32_MAX;
		status = _d3dDevice->CreateSamplerState(&sDesc, &_d3dSampler);
		if (CHECK(status))
			_d3dContext->PSSetSamplers(0, 1, &_d3dSampler);
		else
			Error(_log, "Could not create the sampler. Reason: %x", status);
	}

	if (CHECK(status))
	{
		DirectX::XMFLOAT4 params = { static_cast<float>(_targetMonitorNits), 18.8515625f - 18.6875f * _targetMonitorNits, 0.0, 0.0 };

		D3D11_BUFFER_DESC cbDesc;
		CLEAR(cbDesc);
		cbDesc.ByteWidth = sizeof(params);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA initData;
		CLEAR(initData);
		initData.pSysMem = &params;

		status = _d3dDevice->CreateBuffer(&cbDesc, &initData, &_d3dBuffer);

		if (CHECK(status))
		{
			_d3dContext->VSSetConstantBuffers(0, 1, &_d3dBuffer);
		}
		else
			Error(_log, "Could not create constant buffer. Reason: %x", status);
	}


	if (CHECK(status))
	{
		D3D11_VIEWPORT vp{};
		CLEAR(vp);

		vp.Width = static_cast<FLOAT>(descConvert->Width);
		vp.Height = static_cast<FLOAT>(descConvert->Height);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;

		_d3dContext->RSSetViewports(1, &vp);

		return true;
	}

	return false;
}

HRESULT DxGrabber::deepScaledCopy(ID3D11Texture2D* source)
{
	HRESULT status = S_OK;	
	
	if (_actualDivide >= 0)
	{		
		_d3dContext->CopySubresourceRegion(_d3dConvertTexture, 0, 0, 0, 0, source, 0, NULL);
		_d3dContext->GenerateMips(_d3dConvertTextureView);
		_d3dContext->CopySubresourceRegion(_d3dSourceTexture, 0, 0, 0, 0, _d3dConvertTexture, _actualDivide, NULL);
	}
	else
	{
		D3D11_TEXTURE2D_DESC sourceDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};

		CLEAR(srvDesc);
		CLEAR(sourceDesc);

		source->GetDesc(&sourceDesc);

		if (sourceDesc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && sourceDesc.Format != DXGI_FORMAT_R10G10B10A2_UNORM)
			return E_INVALIDARG;

		srvDesc.Format = sourceDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		ID3D11ShaderResourceView* rv = nullptr;
		status = _d3dDevice->CreateShaderResourceView(source, &srvDesc, &rv);

		if (!CHECK(status))
		{
			if (_warningCounter > 0)
			{
				Error(_log, "CreateShaderResourceView failed (%i). Reason: %i", _warningCounter--, status);
			}
			return status;
		}
		else
			_d3dContext->PSSetShaderResources(0, 1, &rv);

		_d3dContext->Draw(4, 0);
		_d3dContext->CopyResource(_d3dSourceTexture, _d3dConvertTexture);
		SafeRelease(&rv);
	}

	return status;
}
void DxGrabber::grabFrame()
{
	DXGI_OUTDUPL_FRAME_INFO infoFrame{};
	IDXGIResource* resourceDesktop = nullptr;

	auto status = _d3dDuplicate->AcquireNextFrame(0, &infoFrame, &resourceDesktop);

	if (CHECK(status))
	{
		D3D11_MAPPED_SUBRESOURCE internalMap{};
		ID3D11Texture2D* texDesktop = nullptr;

		CLEAR(internalMap);

		status = resourceDesktop->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texDesktop);

		if (CHECK(status) && texDesktop != nullptr)
		{
			if (_hardware)
			{
				status = deepScaledCopy(texDesktop);
			}
			else
			{
				_d3dContext->CopyResource(_d3dSourceTexture, texDesktop);
			}

			if (CHECK(status) && CHECK(_d3dContext->Map(_d3dSourceTexture, 0, D3D11_MAP_READ, 0, &internalMap)))
			{
				processSystemFrameBGRA((uint8_t*)internalMap.pData, (int)internalMap.RowPitch, !(_hardware && _wideGamut));
				_d3dContext->Unmap(_d3dSourceTexture, 0);
			}

			SafeRelease(&texDesktop);
		}
		else if (_warningCounter > 0)
		{
			Error(_log, "ResourceDesktop->QueryInterface failed. Reason: %i", status);
			_warningCounter--;
		}
	}
	else if (status == DXGI_ERROR_WAIT_TIMEOUT)
	{
		if (_warningCounter > 0)
		{
			Debug(_log, "AcquireNextFrame didn't return the frame. Just warning: the screen has not changed?");
			_warningCounter--;
		}

		if (_cacheImage.width() > 1)
		{
			emit SignalNewCapturedFrame(_cacheImage);
		}
	}
	else if (status == DXGI_ERROR_ACCESS_LOST)
	{
		Error(_log, "Lost DirectX capture context. Stopping.");
		_dxRestartNow = true;
	}
	else if (_warningCounter > 0)
	{
		Error(_log, "AcquireNextFrame failed. Reason: %i", status);
		_warningCounter--;
	}

	SafeRelease(&resourceDesktop);

	_d3dDuplicate->ReleaseFrame();

	if (_dxRestartNow)
	{
		uninit();
	}
}


void DxGrabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;
}

void DxGrabber::cacheHandler(const Image<ColorRgb>& image)
{
	_cacheImage = image;
}
