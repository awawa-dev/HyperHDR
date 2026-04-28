#pragma once

#include "ProviderUdp.h"
#include <led-drivers/InfiniteColorEngineRgbw.h>

class DriverNetDDP : public ProviderUdp
{
public:
	explicit DriverNetDDP(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	int writeFiniteColors(bool isRgbw, const int ledsNumber, const std::vector<ColorRgb>& ledValues);
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;

	InfiniteColorEngineRgbw _infiniteColorEngineRgbw;
	bool _enable_ice_rgbw;
	linalg::aliases::float3 _ice_white_temperatur;
	float _ice_white_mixer_threshold;
	float _ice_white_led_intensity;

	bool _isRgbw;
	RGBW::WhiteAlgorithm _whiteAlgorithm;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;

	RGBW::RgbwChannelCorrection channelCorrection{};

	std::vector<uint8_t> _rgbwBuffer;
	std::vector<uint8_t> _ddpFrame;

	static bool isRegistered;
};
