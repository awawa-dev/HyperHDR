#pragma once

// HyperHDR includes
#include "ProviderSpi.h"

class DriverSpiAPA104 : public ProviderSpi
{
public:
	explicit DriverSpiAPA104(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	const int SPI_BYTES_PER_COLOUR;
	const int SPI_FRAME_END_LATCH_BYTES;

	uint8_t bitpair_to_byte[4];

	static bool isRegistered;
};
