#pragma once

#ifndef PCH_ENABLED
	#include <QHostAddress>
	#include <QJsonObject>
	#include <QString>
	#include <vector>
#endif

#include <led-drivers/net/ProviderUdp.h>

class DriverNetWiz : public ProviderUdp
{
	Q_OBJECT

public:
	explicit DriverNetWiz(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

	QJsonObject discover(const QJsonObject& params) override;

protected:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	bool powerOn() override;
	bool powerOff() override;
	void identify(const QJsonObject& params) override;

private:
	struct WizLamp
	{
		QString name;
		QHostAddress ipAddress;
		QString macAddress;
	};

	QByteArray buildSetPilotPacket(int r, int g, int b) const;
	QByteArray buildPowerPacket(bool on) const;

	int _dimming;
	std::vector<WizLamp> _lamps;

	static bool isRegistered;
};

