#pragma once

#include <led-drivers/LedDevice.h>
#include <led-drivers/InfiniteColorEngineRgbw.h>

struct ws2811_t;

class DriverPwmNeopixel : public LedDevice
{
public:
	explicit DriverPwmNeopixel(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(QJsonObject deviceConfig) override;
	int open() override;
	int close() override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;

private:

	std::unique_ptr<ws2811_t> _ledString;
	int			_channel;
	RGBW::WhiteAlgorithm _whiteAlgorithm;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;

	RGBW::RgbwChannelCorrection channelCorrection;

	InfiniteColorEngineRgbw _infiniteColorEngineRgbw;
	bool _enable_ice_rgbw;
	linalg::aliases::float3 _ice_white_temperatur;
	float _ice_white_mixer_threshold;
	float _ice_white_led_intensity;

	static bool isRegistered;
};
