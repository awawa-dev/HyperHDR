#pragma once

#include "ProviderSerial.h"

class DriverSerialAtmo : public ProviderSerial
{
public:
	explicit DriverSerialAtmo(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	static bool isRegistered;
};
