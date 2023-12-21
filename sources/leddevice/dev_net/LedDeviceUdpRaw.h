#pragma once

#include "ProviderUdp.h"

class LedDeviceUdpRaw : public ProviderUdp
{
public:
	explicit LedDeviceUdpRaw(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
};
