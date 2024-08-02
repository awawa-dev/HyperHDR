#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QTcpSocket>
#endif

class QTcpSocket;


#include <led-drivers/LedDevice.h>

class DriverNetFadeCandy : public LedDevice
{
	Q_OBJECT

public:
	explicit DriverNetFadeCandy(const QJsonObject& deviceConfig);
	~DriverNetFadeCandy() override;

	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:

	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int close() override;
	int write(const std::vector<ColorRgb>& ledValues) override;

private:
	bool initNetwork();
	bool tryConnect();
	bool isConnected() const;
	qint64 transferData();
	qint64 sendSysEx(uint8_t systemId, uint8_t commandId, const QByteArray& msg);
	void sendFadeCandyConfiguration();

	QTcpSocket* _client;
	QString     _host;
	int    _port;
	int    _channel;
	QByteArray  _opc_data;

	bool        _setFcConfig;
	double      _gamma;
	double      _whitePoint_r;
	double      _whitePoint_g;
	double      _whitePoint_b;
	bool        _noDither;
	bool        _noInterp;
	bool        _manualLED;
	bool        _ledOnOff;

	static bool isRegistered;
};
