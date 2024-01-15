#pragma once

#include "ProviderSerial.h"

class LedDeviceKarate : public ProviderSerial
{
	Q_OBJECT
public:
	explicit LedDeviceKarate(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
};
