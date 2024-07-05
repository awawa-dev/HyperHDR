#pragma once

#ifndef PCH_ENABLED
	#include <QUuid>
#endif

#include "ProviderUdp.h"

/**
 *
 * https://raw.githubusercontent.com/forkineye/ESPixelStick/master/_E131.h
 * Project: E131 - E.131 (sACN) library for Arduino
 * Copyright (c) 2015 Shelby Merrick
 * http://www.forkineye.com
 *
 *  This program is provided free for you to use in any way that you wish,
 *  subject to the laws and regulations where you are using it.  Due diligence
 *  is strongly suggested before using this code.  Please give credit where due.
 *
 **/

union e131_packet_t;

class DriverNetUdpE131 : public ProviderUdp
{
public:
	explicit DriverNetUdpE131(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	void prepare(unsigned this_universe, unsigned this_dmxChannelCount);

	std::unique_ptr<e131_packet_t> e131_packet;
	uint8_t _e131_seq = 0;
	uint8_t _e131_universe = 1;
	uint8_t _acn_id[12] = { 0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00 };
	QString _e131_source_name;
	QUuid _e131_cid;

	static bool isRegistered;
};
