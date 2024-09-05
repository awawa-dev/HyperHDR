#pragma once

// HyperHDR includes
#include "ProviderSpi.h"

class DriverSpiP9813 : public ProviderSpi
{
public:
	explicit DriverSpiP9813(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	uint8_t calculateChecksum(const ColorRgb& color) const;

	static bool isRegistered;
};
