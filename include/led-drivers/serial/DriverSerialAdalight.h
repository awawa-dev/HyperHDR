#pragma once

#include "ProviderSerial.h"
#include <led-drivers/InfiniteColorEngineRgbw.h>

class DriverSerialAdalight : public ProviderSerial
{
	Q_OBJECT

public:
	explicit DriverSerialAdalight(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	void CreateHeader();
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;

	void whiteChannelExtension(uint8_t*& writer);

	const short _headerSize;
	bool        _ligthBerryAPA102Mode;
	bool		_awa_mode;

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
