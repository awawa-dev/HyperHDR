#pragma once

#include <led-drivers/LedDevice.h>
#include "ProviderRestApi.h"
#include "ProviderUdp.h"

class DriverNetWled : public ProviderUdp
{
	Q_OBJECT

public:
	explicit DriverNetWled(const QJsonObject& deviceConfig);
	~DriverNetWled() override;

	static LedDevice* construct(const QJsonObject& deviceConfig);
	QJsonObject discover(const QJsonObject& params) override;

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	bool powerOn() override;
	bool powerOff() override;

private:
	bool initRestAPI(const QString& hostname, int port);
	QString getOnOffRequest(bool isOn) const;
	ProviderRestApi* _restApi;

	QString _hostname;
	int		_apiPort;
	int		_warlsStreamPort;
	bool	_overrideBrightness;
	int		_brightnessLevel;
	bool	_restoreConfig;
	QJsonDocument _configBackup;

	static bool isRegistered;
};
