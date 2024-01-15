#pragma once

#include "ProviderSerial.h"

class LedDeviceAtmo : public ProviderSerial
{
public:
	explicit LedDeviceAtmo(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
};
