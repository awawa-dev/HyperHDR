#pragma once

// Local HyperHDR includes
#include "ProviderSpi.h"

class DriverSpiLpd6803 : public ProviderSpi
{
public:
	explicit DriverSpiLpd6803(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
