#pragma once

#include "ProviderUdp.h"

class DriverNetUdpRaw : public ProviderUdp
{
public:
	explicit DriverNetUdpRaw(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
