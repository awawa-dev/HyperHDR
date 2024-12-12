#pragma once

#include <led-drivers/LedDevice.h>
#include <linalg.h>

class DriverNetHomeAssistant : public LedDevice
{
	Q_OBJECT

	struct HomeAssistantLamp;

	struct HomeAssistantInstance
	{
		QString homeAssistantHost;
		QString longLivedAccessToken;
		bool restoreOriginalState;

		std::list<HomeAssistantLamp> lamps;
	};

	struct HomeAssistantLamp
	{
		enum Mode { RGB = 0, HSV };

		QString name;
		Mode colorModel;

		struct
		{
			int isPoweredOn = -1;
			int brightness = -1;
			linalg::aliases::int4 color{ -1, -1, -1, -1 };
		} orgState;
	};

public:
	explicit DriverNetHomeAssistant(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

	QJsonObject discover(const QJsonObject& params) override;

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	HomeAssistantInstance _haInstance;

	static bool isRegistered;
};
