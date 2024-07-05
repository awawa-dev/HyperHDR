#pragma once

#include "ProviderSerial.h"

class DriverSerialAdalight : public ProviderSerial
{
	Q_OBJECT

public:
	explicit DriverSerialAdalight(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	void CreateHeader();
	int write(const std::vector<ColorRgb>& ledValues) override;

	void whiteChannelExtension(uint8_t*& writer);

	const short _headerSize;
	bool        _ligthBerryAPA102Mode;
	bool		_awa_mode;

	bool _white_channel_calibration;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;

	static bool isRegistered;
};
