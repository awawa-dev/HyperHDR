#include <led-drivers/net/DriverNetZigbee2mqtt.h>
#include <utils/GlobalSignals.h>
#include <utils/InternalClock.h>

namespace
{
	constexpr auto ZIGBEE_DISCOVERY_MESSAGE = "zigbee2mqtt/bridge/devices";
	constexpr int DEFAULT_TIME_MEASURE_MESSAGE = 25;
	constexpr int DEFAULT_COMMUNICATION_TIMEOUT_MS = 200;
}

DriverNetZigbee2mqtt::DriverNetZigbee2mqtt(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig),
	_discoveryFinished(false),
	_colorsFinished(0),
	_timeLogger(0)
{
}

LedDevice* DriverNetZigbee2mqtt::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetZigbee2mqtt(deviceConfig);
}

bool DriverNetZigbee2mqtt::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	if (LedDevice::init(deviceConfig))
	{

		_zigInstance.transition = deviceConfig["transition"].toInt(0);
		_zigInstance.constantBrightness = deviceConfig["constantBrightness"].toInt(0);
		
		Debug(_log, "Transition (ms)       : %s", (_zigInstance.transition > 0) ? QSTRING_CSTR(QString::number(_zigInstance.transition)) : "disabled" );
		Debug(_log, "ConstantBrightness    : %s", (_zigInstance.constantBrightness > 0) ? QSTRING_CSTR(QString::number(_zigInstance.constantBrightness)) : "disabled");

		auto arr = deviceConfig["lamps"].toArray();

		for (const auto&& lamp : arr)
			if (lamp.isObject())
			{
				Zigbee2mqttLamp hl;
				auto lampObj = lamp.toObject();
				hl.name = lampObj["name"].toString();
				hl.colorModel = static_cast<Zigbee2mqttLamp::Mode>(lampObj["colorModel"].toInt(0));
				Debug(_log, "Configured lamp (%s) : %s", (hl.colorModel == 0) ? "RGB" : "HSV", QSTRING_CSTR(hl.name));
				_zigInstance.lamps.push_back(hl);
			}

		if (arr.size() > 0)
		{
			isInitOK = true;
		}
	}
	return isInitOK;
}


bool DriverNetZigbee2mqtt::powerOnOff(bool isOn)
{
	QJsonDocument doc;	

	for (const auto& lamp : _zigInstance.lamps)
	{
		QString topic = QString("zigbee2mqtt/%1/set").arg(lamp.name);
		QJsonObject row;

		row["state"] = (isOn) ? "ON" : "OFF";
		
		doc.setObject(row);
		emit GlobalSignals::getInstance()->SignalMqttPublish(topic, doc.toJson(QJsonDocument::Compact));		
	}
	
	if (_zigInstance.lamps.size() > 0)
	{
		if (isOn)
		{
			connect(GlobalSignals::getInstance(), &GlobalSignals::SignalMqttReceived, this, &DriverNetZigbee2mqtt::handlerSignalMqttReceived, Qt::DirectConnection);
		}
		else
		{
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalMqttReceived, this, &DriverNetZigbee2mqtt::handlerSignalMqttReceived);
		}
	}

	_timeLogger = 0;
	
	return true;
}

bool DriverNetZigbee2mqtt::powerOn()
{
	return powerOnOff(true);
}

bool DriverNetZigbee2mqtt::powerOff()
{
	return powerOnOff(false);
}

int DriverNetZigbee2mqtt::write(const std::vector<ColorRgb>& ledValues)
{
	QJsonDocument doc;

	_colorsFinished = std::min(ledValues.size(), _zigInstance.lamps.size());

	auto rgb = ledValues.begin();
	for (const auto& lamp : _zigInstance.lamps)
		if (rgb != ledValues.end())
		{
			QJsonObject row;
			auto& color = *(rgb++);
			int brightness = 0;
			
			QString topic = QString("zigbee2mqtt/%1/set").arg(lamp.name);

			if (_zigInstance.transition > 0)
			{
				row["transition"] = _zigInstance.transition / 1000.0;
			}

			if (lamp.colorModel == Zigbee2mqttLamp::Mode::RGB)
			{
				QJsonObject rgb; rgb["r"] = color.red; rgb["g"] = color.green; rgb["b"] = color.blue;
				row["color"] = rgb;
				brightness = std::min(std::max(static_cast<int>(std::roundl(0.2126 * color.red + 0.7152 * color.green + 0.0722 * color.blue)), 0), 255);
			}
			else
			{
				uint16_t h;
				float s, v;
				color.rgb2hsl(color.red, color.green, color.blue, h, s, v);

				QJsonObject hs; hs["hue"] = h; hs["saturation"] = static_cast<int>(std::roundl(s * 100.0));
				row["color"] = hs;
				brightness = std::min(std::max(static_cast<int>(std::roundl(v * 255.0)), 0), 255);
			}

			if (brightness > 0 && _zigInstance.constantBrightness > 0)
			{
				brightness = _zigInstance.constantBrightness;
			}
			
			row["brightness"] = brightness;

			doc.setObject(row);
			emit GlobalSignals::getInstance()->SignalMqttPublish(topic, doc.toJson(QJsonDocument::Compact));
		}

	auto start = InternalClock::nowPrecise();

	std::unique_lock<std::mutex> lck(_mtx);
	_cv.wait_for(lck, std::chrono::milliseconds(DEFAULT_COMMUNICATION_TIMEOUT_MS));

	for (int timeout = 0; timeout < 20 && _colorsFinished > 0 && (InternalClock::nowPrecise() < start + DEFAULT_COMMUNICATION_TIMEOUT_MS); timeout++)
	{
		QThread::msleep(10);
	}

	if (_colorsFinished.exchange(0) > 0)
	{
		Warning(_log, "The communication timed out after %ims (%i)", (int)(InternalClock::nowPrecise() - start), (++_timeLogger));
	}
	else if (_timeLogger >= 0 && _timeLogger < DEFAULT_TIME_MEASURE_MESSAGE)
	{
		Info(_log, "The communication took: %ims (%i/%i)", (int)(InternalClock::nowPrecise() - start), ++_timeLogger, DEFAULT_TIME_MEASURE_MESSAGE);
	}

	return 0;
}

void DriverNetZigbee2mqtt::handlerSignalMqttReceived(QString topic, QString payload)
{
	if (topic == ZIGBEE_DISCOVERY_MESSAGE && !_discoveryFinished)
	{
		_discoveryMessage = payload;
		_discoveryFinished = true;
	}
	else if (_colorsFinished > 0)
	{
		_colorsFinished--;
		if (_colorsFinished == 0)
		{
			_cv.notify_all();
		}
	}
}

QJsonObject DriverNetZigbee2mqtt::discover(const QJsonObject& params)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

	_discoveryFinished = false;
	_discoveryMessage = "";

	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalMqttReceived, this, &DriverNetZigbee2mqtt::handlerSignalMqttReceived, Qt::DirectConnection);
	emit GlobalSignals::getInstance()->SignalMqttSubscribe(false, ZIGBEE_DISCOVERY_MESSAGE);
	emit GlobalSignals::getInstance()->SignalMqttSubscribe(true, ZIGBEE_DISCOVERY_MESSAGE);

	for (int i = 0; i < 15 && !_discoveryFinished; i++)
	{
		QThread::msleep(100);
	}

	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalMqttReceived, this, &DriverNetZigbee2mqtt::handlerSignalMqttReceived);
	emit GlobalSignals::getInstance()->SignalMqttSubscribe(false, ZIGBEE_DISCOVERY_MESSAGE);

	if (!_discoveryFinished)
	{
		Error(_log, "Could not find any Zigbee2mqtt devices. Check HyperHDR MQTT network / MQTT broker / Zigbee2mqtt configuration");
	}
	else
	{
		QJsonDocument doc = QJsonDocument::fromJson(_discoveryMessage.toUtf8());

		if (!doc.isNull())
		{
			if (doc.isArray())
			{
				for (const auto&& device : doc.array())
					if (device.isObject())
					{
						auto item = device.toObject();
						if (!item["friendly_name"].toString().isEmpty() &&
							item.contains("definition") && item["definition"].isObject())
						{
							auto defItem = item["definition"].toObject();
							if (defItem.contains("exposes") && defItem["exposes"].isArray())
							{
								for (const auto&& exposesItem : defItem["exposes"].toArray())
									if (exposesItem.isObject())
									{
										auto exposesObj = exposesItem.toObject();
										if (exposesObj.contains("type") && QString::compare(exposesObj["type"].toString(), "light", Qt::CaseInsensitive) == 0 &&
											exposesObj.contains("features") && exposesObj["features"].isArray())
										{
											for (const auto&& featureItem : exposesObj["features"].toArray())
												if (featureItem.isObject())
												{
													auto features = featureItem.toObject();
													if (features.contains("name"))
													{
														auto name = features["name"].toString();
														QString colorMode;

														if (QString::compare(name, "color_xy", Qt::CaseInsensitive) == 0)
														{
															colorMode = "RGB";
														}
														else if (QString::compare(name, "color_hs", Qt::CaseInsensitive) == 0)
														{
															colorMode = "HSV";
														}

														if (!colorMode.isEmpty())
														{
															QJsonObject newIp;
															newIp["value"] = colorMode;
															newIp["name"] = item["friendly_name"];
															deviceList.push_back(newIp);
															break;
														}
													}
												}
										}
									}
							}

						}
					}
			}
			else
			{
				Error(_log, "Document is not an array");
			}
		}
		else
		{
			Error(_log, "Document is not an JSON");
		}
	}

	devicesDiscovered.insert("devices", deviceList);
	Debug(_log, "devicesDiscovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

void DriverNetZigbee2mqtt::identify(const QJsonObject& params)
{
	if (params.contains("name") && params.contains("type"))
	{
		auto name = params["name"].toString();
		auto type = params["type"].toString();
		if (!name.isEmpty() && !type.isEmpty())
		{
			Debug(_log, "Testing lamp %s (%s)", QSTRING_CSTR(name), QSTRING_CSTR(type));
			QString topic = QString("zigbee2mqtt/%1/set").arg(name);
			QJsonDocument doc;
			QJsonObject rowOn; rowOn["state"] = "ON";
			QJsonObject rowOff; rowOff["state"] = "OFF";
			QJsonObject colorRGB, rgb; colorRGB["brightness"] = 255; rgb["r"] = 255; rgb["g"] = 0; rgb["b"] = 0; colorRGB["color"] = rgb;
			QJsonObject colorHS, hs; colorHS["brightness"] = 255; hs["hue"] = 360; hs["saturation"] = 100; colorHS["color"] = hs;
			

			doc.setObject(rowOn);
			emit GlobalSignals::getInstance()->SignalMqttPublish(topic, doc.toJson(QJsonDocument::Compact));
			QThread::msleep(300);

			if (type == "RGB")
			{
				doc.setObject(colorRGB);
				emit GlobalSignals::getInstance()->SignalMqttPublish(topic, doc.toJson(QJsonDocument::Compact));
			}
			else
			{
				doc.setObject(colorHS);
				emit GlobalSignals::getInstance()->SignalMqttPublish(topic, doc.toJson(QJsonDocument::Compact));
			}
			QThread::msleep(700);

			doc.setObject(rowOff);
			emit GlobalSignals::getInstance()->SignalMqttPublish(topic, doc.toJson(QJsonDocument::Compact));
		}
	}
}

bool DriverNetZigbee2mqtt::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("zigbee2mqtt", "leds_group_2_network", DriverNetZigbee2mqtt::construct);
