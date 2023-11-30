#pragma once

#include <leddevice/LedDevice.h>

struct ws2811_t;

class LedDeviceWS281x : public LedDevice
{
public:
	explicit LedDeviceWS281x(const QJsonObject& deviceConfig);
	~LedDeviceWS281x() override;
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int close() override;
	int write(const std::vector<ColorRgb>& ledValues) override;

private:

	std::unique_ptr<ws2811_t> _ledString;
	int			_channel;
	RGBW::WhiteAlgorithm _whiteAlgorithm;
	ColorRgbw	_temp_rgbw;
};
