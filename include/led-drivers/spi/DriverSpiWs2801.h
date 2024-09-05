#pragma once

#include "ProviderSpi.h"

class DriverSpiWs2801 : public ProviderSpi
{
public:
	explicit DriverSpiWs2801(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
