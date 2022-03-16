#pragma once

#include <Guiddef.h>
#include <windows.h>
#include <Winerror.h>
#include <dxgi1_2.h>
#include <d3d11.h>

// stl includes
#include <vector>
#include <map>
#include <chrono>

// Qt includes
#include <QObject>
#include <QSocketNotifier>
#include <QRectF>
#include <QMap>
#include <QMultiMap>
#include <QTimer>
#include <QSemaphore>

// util includes
#include <utils/PixelFormat.h>
#include <base/Grabber.h>
#include <utils/Components.h>

// general JPEG decoder includes
#include <QImage>
#include <QColor>


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

public slots:

	bool start() override;

	void stop() override;

	void newWorkerFrame(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) override;

	void newWorkerFrameError(unsigned int workerIndex, QString error, quint64 sourceCount) override;

	void restart();

private:
	QString GetSharedLut();

	void enumerateDevices(bool silent);

	void loadLutFile(PixelFormat color = PixelFormat::NO_CHANGE);

	void getDevices();

	bool init() override;

	void uninit() override;

	bool init_device(QString selectedDeviceName);

private:
	QString					_configurationPath;
	QTimer					_timer;
	QSemaphore				_semaphore;
	int						_warningCounter;

	bool					_dxRestartNow;
	bool					_d3dCache;
	ID3D11Device*			_d3dDevice;
	ID3D11DeviceContext*	_d3dContext;
	ID3D11Texture2D*		_sourceTexture;
	IDXGIOutputDuplication* _d3dDuplicate;
};
