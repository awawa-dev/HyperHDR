#pragma once

#include "ProviderSerial.h"

class DriverSerialKarate : public ProviderSerial
{
	Q_OBJECT
public:
	explicit DriverSerialKarate(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
