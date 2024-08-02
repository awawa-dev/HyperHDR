#pragma once

#ifndef PCH_ENABLED
	#include <QHostAddress>
#endif

#include <led-drivers/LedDevice.h>
#include <utils/Logger.h>

class QUdpSocket;

class ProviderUdp : public LedDevice
{
public:
	ProviderUdp(const QJsonObject& deviceConfig);
	~ProviderUdp() override;

	QHostAddress getAddress() const { return _address; }

protected:

	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int close() override;
	int writeBytes(const unsigned size, const uint8_t* data);
	int writeBytes(const QByteArray& bytes);
	void setPort(int port);

	std::unique_ptr<QUdpSocket> _udpSocket;
	QHostAddress _address;
	quint16       _port;
	QString      _defaultHost;
};
