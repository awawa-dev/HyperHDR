#pragma once

#include "ProviderSpi.h"

class DriverSpiWs2812SPI : public ProviderSpi
{
public:
	explicit DriverSpiWs2812SPI(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	const int SPI_BYTES_PER_COLOUR;

	uint8_t bitpair_to_byte[4];

	static bool isRegistered;
};
