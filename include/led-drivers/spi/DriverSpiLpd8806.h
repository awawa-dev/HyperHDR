#pragma once

// Local HyperHDR includes
#include "ProviderSpi.h"

class DriverSpiLpd8806 : public ProviderSpi
{
public:
	explicit DriverSpiLpd8806(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
