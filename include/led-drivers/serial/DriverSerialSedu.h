#pragma once

#include "ProviderSerial.h"

class DriverSerialSedu : public ProviderSerial
{
public:
	explicit DriverSerialSedu(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
