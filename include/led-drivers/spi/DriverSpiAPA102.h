#pragma once

// HyperHDR includes
#include "ProviderSpi.h"

class DriverSpiAPA102 : public ProviderSpi
{
public:
	explicit DriverSpiAPA102(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	void createHeader();

	static bool isRegistered;
};
