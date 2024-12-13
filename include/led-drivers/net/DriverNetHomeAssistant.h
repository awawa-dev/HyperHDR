#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <memory>
	#include <list>	
#endif

#include <led-drivers/LedDevice.h>
#include <led-drivers/net/ProviderRestApi.h>
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

protected:
	bool powerOn() override;
	bool powerOff() override;

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	bool powerOnOff(bool isOn);

	HomeAssistantInstance _haInstance;

	std::unique_ptr<ProviderRestApi> _restApi;

	static bool isRegistered;
};
