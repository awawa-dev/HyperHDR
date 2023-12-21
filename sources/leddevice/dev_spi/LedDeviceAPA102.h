#pragma once

// HyperHDR includes
#include "ProviderSpi.h"

class LedDeviceAPA102 : public ProviderSpi
{
public:
	explicit LedDeviceAPA102(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	void createHeader();
};
