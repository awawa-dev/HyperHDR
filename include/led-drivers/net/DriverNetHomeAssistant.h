#pragma once

#include <led-drivers/LedDevice.h>

class DriverNetHomeAssistant : public LedDevice
{
	Q_OBJECT

public:
	explicit DriverNetHomeAssistant(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	static bool isRegistered;
};
