#pragma once

#include "ProviderUdp.h"

union artnet_packet_t;

class DriverNetUdpArtNet : public ProviderUdp
{
public:
	explicit DriverNetUdpArtNet(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	void prepare(unsigned this_universe, unsigned this_sequence, unsigned this_dmxChannelCount);

	std::unique_ptr<artnet_packet_t> artnet_packet;
	uint8_t _artnet_seq = 1;
	int _artnet_channelsPerFixture = 3;
	int _artnet_universe = 1;
	bool _disableSplitting = false;

	static bool isRegistered;
};
