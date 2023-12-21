#pragma once

#include <leddevice/LedDevice.h>

class LedDeviceTemplate : public LedDevice
{
public:
	explicit LedDeviceTemplate(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int close() override;
	int write(const std::vector<ColorRgb>& ledValues) override;

private:

};
