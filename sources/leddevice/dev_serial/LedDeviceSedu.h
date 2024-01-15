#pragma once

#include "ProviderSerial.h"

class LedDeviceSedu : public ProviderSerial
{
public:
	explicit LedDeviceSedu(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
};
