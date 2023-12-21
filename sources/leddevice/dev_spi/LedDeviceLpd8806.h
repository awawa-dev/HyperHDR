#pragma once

// Local HyperHDR includes
#include "ProviderSpi.h"

class LedDeviceLpd8806 : public ProviderSpi
{
public:
	explicit LedDeviceLpd8806(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
};
