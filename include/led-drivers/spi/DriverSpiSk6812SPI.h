#pragma once

// HyperHDR includes
#include "ProviderSpi.h"
#include <led-drivers/ColorRgbw.h>

class DriverSpiSk6812SPI : public ProviderSpi
{
public:
	explicit DriverSpiSk6812SPI(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	RGBW::WhiteAlgorithm _whiteAlgorithm;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;

	RGBW::RgbwChannelCorrection channelCorrection;

	const int SPI_BYTES_PER_COLOUR;
	uint8_t bitpair_to_byte[4];	

	static bool isRegistered;
};
