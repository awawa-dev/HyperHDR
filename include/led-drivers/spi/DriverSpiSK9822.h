#pragma once

#include "ProviderSpi.h"

class DriverSpiSK9822 : public ProviderSpi
{
public:
	explicit DriverSpiSK9822(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	void bufferWithMaxCurrent(std::vector<uint8_t>& txBuf, const std::vector<ColorRgb>& ledValues, const int maxLevel);
	void bufferWithAdjustedCurrent(std::vector<uint8_t>& txBuf, const std::vector<ColorRgb>& ledValues, const int threshold, const int maxLevel);

	int _globalBrightnessControlThreshold;
	int _globalBrightnessControlMaxLevel;
	inline __attribute__((always_inline)) unsigned scale(const uint8_t value, const int maxLevel, const uint16_t brightness);
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
