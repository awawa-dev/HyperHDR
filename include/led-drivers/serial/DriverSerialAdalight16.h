#pragma once

#include "ProviderSerial.h"

#include <led-drivers/InfiniteColorEngineRgbw.h>

class DriverSerialAdalight16 : public ProviderSerial
{
	Q_OBJECT

public:
	explicit DriverSerialAdalight16(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	void createHeader();
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	void whiteChannelExtension(uint8_t*& writer) const;
	void writeColorAs16Bit(uint8_t*& writer, const ColorRgb& rgb) const;
	void writeColorAs16Bit(uint8_t*& writer, const linalg::aliases::float3& rgb) const;

	const short _headerSize;
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
