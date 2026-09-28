#pragma once

#include "ProviderSerial.h"

class DriverSerialSP616E : public ProviderSerial
{
public:
	explicit DriverSerialSP616E(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
