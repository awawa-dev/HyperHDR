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
	static LedDevice* construct(const QJsonObject& deviceConfig);

	QJsonObject discover(const QJsonObject& params) override;
	QJsonObject getProperties(const QJsonObject& params) override;
	void identify(const QJsonObject& params) override;

protected:
	bool init(QJsonObject deviceConfig) override;
	int open() override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	bool powerOn() override;
	bool powerOff() override;

private:
	bool initRestAPI(const QString& hostname, int port, const QString& token);
	bool initLedsConfiguration();
	bool loadPanelIds(const QJsonArray& positionData);
	bool loadLightstripIds(int ledCount);
	bool applyConfiguredLedRange();
	int queryNumLeds();
	int writeStreamBatch(const std::vector<ColorRgb>& ledValues, int startIndex, int count, int& ledCounter);
	QJsonDocument changeToExternalControlMode();
	bool applyStreamMasterBrightness();
	QString getOnOffRequest(bool isOn) const;
	ColorRgb candyColor(const ColorRgb& color) const;
	bool resolveApiEndpoint(const QString& host, QString& apiHost, int& apiPort) const;

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
	bool _isLightstrip;
	int _streamLedsPerDatagram;
	int _streamBatchGapMs;

	QVector<int> _panelIds;

	static bool isRegistered;
};
