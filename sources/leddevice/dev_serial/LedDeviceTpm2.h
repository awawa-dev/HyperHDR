#pragma once

#include "ProviderSerial.h"

class LedDeviceTpm2 : public ProviderSerial
{
public:

	explicit LedDeviceTpm2(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
};

