#pragma once

#include "ProviderSpi.h"

class LedDeviceWs2801 : public ProviderSpi
{
public:
	explicit LedDeviceWs2801(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
};
