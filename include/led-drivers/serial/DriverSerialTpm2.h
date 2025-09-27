#pragma once

#include "ProviderSerial.h"

class DriverSerialTpm2 : public ProviderSerial
{
public:

	explicit DriverSerialTpm2(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};

