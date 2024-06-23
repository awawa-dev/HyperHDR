#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <QThread>
	#include <QHostAddress>
	#include <QUdpSocket>

	#include <list>
	#include <string.h>
	#include <cstring>
	#include <chrono>
#endif

#include <QDtls>

#include <led-drivers/LedDevice.h>
#include <utils/Logger.h>

class ProviderUdpSSL : public LedDevice
{
	Q_OBJECT

public:
	ProviderUdpSSL(const QJsonObject& deviceConfig);
	virtual ~ProviderUdpSSL();

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int closeNetwork();
	bool initNetwork();	
	void writeBytes(unsigned int size, const uint8_t* data, bool flush = false);
	virtual std::list<QString> getCiphersuites();

public slots:
	void pskRequired(QSslPreSharedKeyAuthenticator* authenticator);
	void handshakeTimeout();
	void errorHandling(QString message);

private:
	QString      _transport_type;
	QString      _custom;
	QHostAddress _address;
	QString      _defaultHost;
	int          _port;
	int          _ssl_port;
	QString      _server_name;
	QString      _psk;
	QString      _psk_identity;
	int          _handshake_attempts;
	int          _handshake_attempts_left;
	bool         _streamReady;
	uint32_t     _handshake_timeout_min;
	uint32_t     _handshake_timeout_max;
	QDtls*       _dtls;
	QUdpSocket*  _socket;
};
