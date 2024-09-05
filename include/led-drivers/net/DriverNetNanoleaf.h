#pragma once

#ifndef PCH_ENABLED
	#include <QString>
#endif

#include <led-drivers/LedDevice.h>
#include "ProviderRestApi.h"
#include "ProviderUdp.h"



class DriverNetNanoleaf : public ProviderUdp
{
public:
	explicit DriverNetNanoleaf(const QJsonObject& deviceConfig);
	~DriverNetNanoleaf() override;

	static LedDevice* construct(const QJsonObject& deviceConfig);
	QJsonObject discover(const QJsonObject& params) override;
	QJsonObject getProperties(const QJsonObject& params) override;
	void identify(const QJsonObject& params) override;

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	bool powerOn() override;
	bool powerOff() override;

private:
	bool initRestAPI(const QString& hostname, int port, const QString& token);
	bool initLedsConfiguration();
	QJsonDocument changeToExternalControlMode();
	QString getOnOffRequest(bool isOn) const;

	std::unique_ptr<ProviderRestApi> _restApi;

	QString _hostname;
	int  _apiPort;
	QString _authToken;

	bool _topDown;
	bool _leftRight;
	int _startPos;
	int _endPos;
	
	QString _deviceModel;
	QString _deviceFirmwareVersion;
	ushort _extControlVersion;

	int _panelLedCount;

	QVector<int> _panelIds;

	static bool isRegistered;
};

