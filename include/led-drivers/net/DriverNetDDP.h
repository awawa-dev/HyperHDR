#pragma once

#include "ProviderUdp.h"

class DriverNetDDP : public ProviderUdp
{
public:
	explicit DriverNetDDP(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	bool _isRgbw;
	RGBW::WhiteAlgorithm _whiteAlgorithm;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;

	RGBW::RgbwChannelCorrection channelCorrection{};

	std::vector<uint8_t> _rgbwBuffer;

	static bool isRegistered;
};
