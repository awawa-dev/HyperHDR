#pragma once

#include "ProviderRestApi.h"
#include "DriverNetDDP.h"

class DriverNetHyperk : public DriverNetDDP
{
public:
	explicit DriverNetHyperk(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

	QJsonObject discover(const QJsonObject& params) override;

private:
	QString getOnOffRequest(bool isOn) const;

protected:
	bool init(QJsonObject deviceConfig) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	bool powerOn() override;
	bool powerOff() override;

	std::unique_ptr<ProviderRestApi> _restApi;	
	QJsonDocument _configBackup;	
	bool _overrideBrightness;
	int  _brightnessLevel;
	bool _restoreConfig;

	static bool isRegistered;
};
