#pragma once

#include "ProviderUdp.h"
#include <led-drivers/InfiniteColorEngineRgbw.h>

union artnet_packet_t;

class DriverNetUdpArtNet : public ProviderUdp
{
public:
	explicit DriverNetUdpArtNet(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;
	void prepare(unsigned this_universe, unsigned this_sequence, unsigned this_dmxChannelCount);

	std::unique_ptr<artnet_packet_t> artnet_packet;
	uint8_t _artnet_seq = 1;
	int _artnet_channelsPerFixture = 3;
	int _artnet_universe = 1;
	bool _disableSplitting = false;

	InfiniteColorEngineRgbw _infiniteColorEngineRgbw;
	bool _enable_ice_rgbw;
	linalg::aliases::float3 _ice_white_temperatur;
	float _ice_white_mixer_threshold;
	float _ice_white_led_intensity;

	static bool isRegistered;
};
