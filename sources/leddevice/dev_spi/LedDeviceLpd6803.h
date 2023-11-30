#pragma once

// Local HyperHDR includes
#include "ProviderSpi.h"

class LedDeviceLpd6803 : public ProviderSpi
{
public:
	explicit LedDeviceLpd6803(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
};
