#pragma once

#ifndef PCH_ENABLED
	#include <QHostAddress>
	#include <QVector>
#endif

#include <led-drivers/LedDevice.h>

class QUdpSocket;

class DriverNetAtmoOrb : public LedDevice
{
	Q_OBJECT

public:
	explicit DriverNetAtmoOrb(const QJsonObject& deviceConfig);
	~DriverNetAtmoOrb() override;

	static LedDevice* construct(const QJsonObject& deviceConfig);
	QJsonObject discover(const QJsonObject& params) override;
	virtual void identify(const QJsonObject& params) override;

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int close() override;
	int write(const std::vector<ColorRgb>& ledValues) override;

private:
	void setColor(int orbId, const ColorRgb& color, int commandType);
	void sendCommand(const QByteArray& bytes);

	QUdpSocket* _udpSocket;
	QHostAddress _groupAddress;
	QString _multicastGroup;
	quint16 _multiCastGroupPort;
	bool _joinedMulticastgroup;
	bool _useOrbSmoothing;
	int _skipSmoothingDiff;
	QVector<int> _orbIds;
	QMap<int, int> lastColorRedMap;
	QMap<int, int> lastColorGreenMap;
	QMap<int, int> lastColorBlueMap;
	QMultiMap<int, QHostAddress> _services;
	static bool isRegistered;
};
