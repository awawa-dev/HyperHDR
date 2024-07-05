#pragma once

// HyperHDR includes
#include "ProviderSpi.h"

class DriverSpiHyperSPI : public ProviderSpi
{
public:
	explicit DriverSpiHyperSPI(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	void createHeader();
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	void whiteChannelExtension(uint8_t*& writer);

	int _headerSize;
	bool _white_channel_calibration;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;

	static bool isRegistered;
};
