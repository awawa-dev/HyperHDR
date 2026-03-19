#pragma once

// HyperHDR includes
#include "ProviderSpi.h"
#include <led-drivers/InfiniteColorEngineRgbw.h>

class DriverSpiHyperSPI : public ProviderSpi
{
public:
	explicit DriverSpiHyperSPI(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

	int open() override;
	int close() override;

private:
	void createHeader();
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;
	void whiteChannelExtension(uint8_t*& writer);

	int _headerSize;

	InfiniteColorEngineRgbw _infiniteColorEngineRgbw;
	bool _enable_ice_rgbw;
	linalg::aliases::float3 _ice_white_temperatur;
	float _ice_white_mixer_threshold;
	float _ice_white_led_intensity;

	bool _white_channel_calibration;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;

	static bool isRegistered;
};
