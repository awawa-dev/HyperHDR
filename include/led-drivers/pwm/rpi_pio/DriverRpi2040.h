#pragma once

#include <led-drivers/LedDevice.h>
#include <QFile>

class DriverRpi2040 : public LedDevice
{
public:
	explicit DriverRpi2040(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(QJsonObject deviceConfig) override;
	int open() override;
	int close() override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

private:
	std::unique_ptr<QFile> _renderer;
	QString _output;
	bool _isRgbw;
	RGBW::WhiteAlgorithm _whiteAlgorithm;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;

	RGBW::RgbwChannelCorrection channelCorrection;

	static bool isRegistered;
};
