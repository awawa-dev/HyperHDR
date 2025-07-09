#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <QJsonArray>
	#include <memory>
	#include <list>
	#include <atomic>
	#include <chrono>
#endif

#include <led-drivers/LedDevice.h>
#include <lut-calibrator/VectorHelper.h>

class DriverNetZigbee2mqtt : public LedDevice
{
	Q_OBJECT

	struct Zigbee2mqttLamp;

	struct Zigbee2mqttInstance
	{
		int transition;
		int constantBrightness = 255;

		std::list<Zigbee2mqttLamp> lamps;
	};

	struct Zigbee2mqttLamp
	{
		enum Mode { RGB = 0, HSV };

		QString name;
		Mode colorModel;
		int currentBrightness = 255;
	};

public:
	explicit DriverNetZigbee2mqtt(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

	QJsonObject discover(const QJsonObject& params) override;

	void identify(const QJsonObject& params) override;

public slots:
	void handlerSignalMqttReceived(QString topic, QString payload);

protected:
	bool powerOn() override;
	bool powerOff() override;

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	bool powerOnOff(bool isOn);

	Zigbee2mqttInstance	_zigInstance;
	std::atomic<bool>	_discoveryFinished;
	int					_timeLogger;
	QString				_discoveryMessage;
	int					_mqttId;
	long long			_lastUpdate;

	static int mqttId;
	static bool isRegistered;
};
