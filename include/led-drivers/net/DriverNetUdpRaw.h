#pragma once

#include "ProviderUdp.h"

class DriverNetUdpRaw : public ProviderUdp
{
public:
	explicit DriverNetUdpRaw(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
