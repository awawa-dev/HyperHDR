#pragma once

#include "ProviderSerial.h"

class DriverSerialDMX : public ProviderSerial
{
public:
	explicit DriverSerialDMX(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	int _dmxDeviceType = 0;
	int _dmxStart = 1;
	int _dmxSlotsPerLed = 3;
	int _dmxLedCount = 0;
	unsigned int _dmxChannelCount = 0;

	static bool isRegistered;
};
