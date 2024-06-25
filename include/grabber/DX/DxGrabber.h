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
#endif

// util includes
#include <utils/PixelFormat.h>
#include <base/Grabber.h>
#include <utils/Components.h>

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
	QString GetSharedLut();

	void enumerateDevices(bool silent);

	void loadLutFile(PixelFormat color = PixelFormat::NO_CHANGE);

	void getDevices();

	bool init() override;

	void uninit() override;

	bool initDirectX(QString selectedDeviceName);

	bool initShaders();
	HRESULT deepScaledCopy(ID3D11Texture2D* source);

	QString					_configurationPath;
	QTimer*					_timer;
	QTimer*					_retryTimer;
	int						_warningCounter;
	int						_actualDivide;
	bool					_wideGamut;

	bool					_dxRestartNow;
	ID3D11Device*			_d3dDevice;
	ID3D11DeviceContext*	_d3dContext;
	ID3D11Buffer*			_d3dBuffer;
	ID3D11SamplerState*		_d3dSampler;
	ID3D11InputLayout*		_d3dVertexLayout;
	ID3D11VertexShader*		_d3dVertexShader;
	ID3D11PixelShader*		_d3dPixelShader;
	ID3D11Texture2D*		_d3dSourceTexture;
	ID3D11Texture2D*		_d3dConvertTexture;
	ID3D11ShaderResourceView* _d3dConvertTextureView;
	ID3D11RenderTargetView* _d3dRenderTargetView;
	IDXGIOutputDuplication* _d3dDuplicate;
	DXGI_OUTDUPL_DESC		_surfaceProperties;
	Image<ColorRgb>			_cacheImage;
};
