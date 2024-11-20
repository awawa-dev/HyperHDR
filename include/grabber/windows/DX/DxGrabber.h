#pragma once

#include <Guiddef.h>
#include <windows.h>
#include <Winerror.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d11_1.h>

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QSocketNotifier>
	#include <QRectF>
	#include <QMap>
	#include <QMultiMap>
	#include <QTimer>

	#include <vector>
	#include <map>
	#include <chrono>
	#include <list>
	#include <algorithm>
#endif

// util includes
#include <utils/PixelFormat.h>
#include <base/Grabber.h>
#include <utils/Components.h>

template <class T> void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

struct DisplayHandle
{
	QString name;
	int warningCounter = 6;
	bool wideGamut = false;
	int actualDivide = -1, actualWidth = 0, actualHeight = 0;
	uint targetMonitorNits = 0;
	ID3D11Texture2D* d3dConvertTexture = nullptr;
	ID3D11RenderTargetView* d3dRenderTargetView = nullptr;
	ID3D11ShaderResourceView* d3dConvertTextureView = nullptr;
	ID3D11VertexShader* d3dVertexShader = nullptr;
	ID3D11PixelShader* d3dPixelShader = nullptr;
	ID3D11Buffer* d3dBuffer = nullptr;
	ID3D11SamplerState* d3dSampler = nullptr;
	ID3D11InputLayout* d3dVertexLayout = nullptr;
	IDXGIOutputDuplication* d3dDuplicate = nullptr;
	ID3D11Texture2D* d3dSourceTexture = nullptr;
	DXGI_OUTDUPL_DESC surfaceProperties{};

	DisplayHandle() = default;
	DisplayHandle(const DisplayHandle&) = delete;
	~DisplayHandle()
	{
		SafeRelease(&d3dRenderTargetView);
		SafeRelease(&d3dSourceTexture);
		SafeRelease(&d3dConvertTextureView);
		SafeRelease(&d3dConvertTexture);
		SafeRelease(&d3dVertexShader);
		SafeRelease(&d3dVertexLayout);
		SafeRelease(&d3dPixelShader);
		SafeRelease(&d3dSampler);
		SafeRelease(&d3dBuffer);
		SafeRelease(&d3dDuplicate);
		printf("SmartPointer is removing: DisplayHandle for %s\n", QSTRING_CSTR(name));
	};
};

class DxGrabber : public Grabber
{
	Q_OBJECT

public:

	DxGrabber(const QString& device, const QString& configurationPath);

	~DxGrabber();

	void setHdrToneMappingEnabled(int mode) override;

	void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom) override;

public slots:

	void grabFrame();

	void cacheHandler(const Image<ColorRgb>& image);

	bool start() override;

	void stop() override;

	void newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) override;

	void newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount) override;

	void restart();

private:

	const QString MULTI_MONITOR = "MULTI-MONITOR";

	void captureFrame(DisplayHandle& display);

	int captureFrame(DisplayHandle& display, Image<ColorRgb>& image);

	QString GetSharedLut();

	void enumerateDevices(bool silent);

	void loadLutFile(PixelFormat color = PixelFormat::NO_CHANGE, bool silent = false);

	void getDevices();

	bool init() override;

	void uninit() override;

	bool initDirectX(QString selectedDeviceName);

	bool initShaders(DisplayHandle& display);
	HRESULT deepScaledCopy(DisplayHandle& display, ID3D11Texture2D* source);

	QString					_configurationPath;
	QTimer*					_timer;
	QTimer*					_retryTimer;
	bool					_multiMonitor;

	bool					_dxRestartNow;
	std::list<std::unique_ptr<DisplayHandle>> _handles;
	ID3D11Device*			_d3dDevice;
	ID3D11DeviceContext*	_d3dContext;
	Image<ColorRgb>			_cacheImage;
};
