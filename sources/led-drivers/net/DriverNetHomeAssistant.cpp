#include <led-drivers/net/DriverNetHomeAssistant.h>
#include <utils/InternalClock.h>

#include <HyperhdrConfig.h>
#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>
#endif

DriverNetHomeAssistant::DriverNetHomeAssistant(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig),
	_lastUpdate(0)
{
}

LedDevice* DriverNetHomeAssistant::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetHomeAssistant(deviceConfig);
}

bool DriverNetHomeAssistant::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	if (LedDevice::init(deviceConfig))
	{

		_haInstance.homeAssistantHost = deviceConfig["homeAssistantHost"].toString();
		_haInstance.longLivedAccessToken = deviceConfig["longLivedAccessToken"].toString();
		_haInstance.transition = deviceConfig["transition"].toInt(0);
		_haInstance.constantBrightness = deviceConfig["constantBrightness"].toInt(255);
		_haInstance.restoreOriginalState = deviceConfig["restoreOriginalState"].toBool(false);
		_maxRetry = deviceConfig["maxRetry"].toInt(60);

		QUrl url("http://" + _haInstance.homeAssistantHost);
		_restApi = std::make_unique<ProviderRestApi>(url.host(), url.port(8123));		
		_restApi->addHeader("Authorization", QString("Bearer %1").arg(_haInstance.longLivedAccessToken));
		
		Debug(_log, "HomeAssistantHost     : %s", QSTRING_CSTR(_haInstance.homeAssistantHost));
		Debug(_log, "RestoreOriginalState  : %s", (_haInstance.restoreOriginalState) ? "yes" : "no");
		Debug(_log, "Transition (ms)       : %s", (_haInstance.transition > 0) ? QSTRING_CSTR(QString::number(_haInstance.transition)) : "disabled" );
		Debug(_log, "ConstantBrightness    : %s", (_haInstance.constantBrightness > 0) ? QSTRING_CSTR(QString::number(_haInstance.constantBrightness)) : "disabled");
		Debug(_log, "Max retry             : %d", _maxRetry);

		auto arr = deviceConfig["lamps"].toArray();

		for (const auto&& lamp : arr)
			if (lamp.isObject())
			{
				HomeAssistantLamp hl;
				auto lampObj = lamp.toObject();
				hl.name = lampObj["name"].toString();
				hl.colorModel = static_cast<HomeAssistantLamp::Mode>(lampObj["colorModel"].toInt(0));				
				hl.currentBrightness = _haInstance.constantBrightness;
				Debug(_log, "Configured lamp (%s) : %s", (hl.colorModel == 0) ? "RGB" : "HSV", QSTRING_CSTR(hl.name));
				_haInstance.lamps.push_back(hl);
			}

		if (_haInstance.homeAssistantHost.length() > 0 && _haInstance.longLivedAccessToken.length() > 0 && arr.size() > 0)
		{
			isInitOK = true;
		}
	}
	return isInitOK;
}


bool DriverNetHomeAssistant::powerOnOff(bool isOn)
{
	QJsonDocument doc;
	QJsonObject row;
	QJsonArray entities;

	for (const auto& lamp : _haInstance.lamps)
	{
		entities.push_back(lamp.name);
	}

	row["entity_id"] = entities;
	doc.setObject(row);

	QString message(doc.toJson(QJsonDocument::Compact));
	_restApi->setBasePath(QString("/api/services/light/%1").arg((isOn) ? "turn_on" : "turn_off"));
	auto response = _restApi->post(message);

	if (response.error())
	{
		this->setInError(response.error() ? response.getErrorReason() : "Unknown");
		setupRetry(5000);
		return false;
	}

	return true;
}

bool DriverNetHomeAssistant::powerOn()
{
	if (_haInstance.restoreOriginalState)
	{
		if (!saveStates())
			return false;
	}
	_lastUpdate = InternalClock::now() - 9000;
	return powerOnOff(true);
}

bool DriverNetHomeAssistant::powerOff()
{
	if (_haInstance.restoreOriginalState)
	{
		restoreStates();
		return true;
	}
	return powerOnOff(false);
}

int DriverNetHomeAssistant::write(const std::vector<ColorRgb>& ledValues)
{
	QJsonDocument doc;
	auto start = InternalClock::now();
	auto lastUpdate = _lastUpdate;

	auto rgb = ledValues.begin();
	for (auto& lamp : _haInstance.lamps)
		if (rgb != ledValues.end())
		{
			QJsonObject row;
			auto& color = *(rgb++);
			int brightness = 0;
			
			row["entity_id"] = lamp.name;

			if (_haInstance.transition > 0)
			{
				row["transition"] = _haInstance.transition / 1000.0;
			}

			if (lamp.colorModel == HomeAssistantLamp::Mode::RGB)
			{
				row["rgb_color"] = QJsonArray{ color.red, color.green, color.blue };
				brightness = std::min(std::max(static_cast<int>(std::roundl(0.2126 * color.red + 0.7152 * color.green + 0.0722 * color.blue)), 0), 255);
			}
			else
			{
				uint16_t h;
				float s, v;
				color.rgb2hsl(color.red, color.green, color.blue, h, s, v);
				row["hs_color"] = QJsonArray{ h, static_cast<int>(std::roundl(s * 100.0)) };
				brightness = std::min(std::max(static_cast<int>(std::roundl(v * 255.0)), 0), 255);
			}

			if (_haInstance.constantBrightness == 0)
			{
				row["brightness"] = lamp.currentBrightness = brightness;
			}
			else if (lamp.currentBrightness <= 0 && brightness > 0)
			{
				row["brightness"] = lamp.currentBrightness = _haInstance.constantBrightness;
			}
			else if (lamp.currentBrightness > 0 && brightness == 0)
			{
				row["brightness"] = lamp.currentBrightness = 0;
			}
			else if (start - lastUpdate >= 10000)
			{
				_lastUpdate = start;
				row["brightness"] = lamp.currentBrightness;
			}

			doc.setObject(row);
			QString message(doc.toJson(QJsonDocument::Compact));
			_restApi->setBasePath("/api/services/light/turn_on");
			auto response = _restApi->post(message);

			if (response.error())
			{
				this->setInError(response.error() ? response.getErrorReason() : "Unknown");
				setupRetry(5000);
				return false;
			}
		}

	return 0;
}

bool DriverNetHomeAssistant::saveStates()
{
	for (auto& lamp : _haInstance.lamps)
	{
		_restApi->setBasePath(QString("/api/states/%1").arg(lamp.name));
		auto response = _restApi->get();
		if (response.error())
		{
			this->setInError(response.error() ? response.getErrorReason() : "Unknown");
			setupRetry(5000);
			return false;
		}
		auto body = response.getBody();
		if (body.isEmpty() || !body.isObject())
		{
			Error(_log, "The current state of the light %s is unknown", QSTRING_CSTR(lamp.name));
			continue;
		}

		// read state
		auto obj = body.object();
		if (obj.contains("state") && !obj["state"].isNull())
		{
			lamp.orgState.isPoweredOn = QString::compare(obj["state"].toString("off"), QString("on"), Qt::CaseInsensitive) == 0;
		}
		else
		{
			lamp.orgState.isPoweredOn = -1;
		}

		if (obj.contains("attributes") && obj["attributes"].isObject())
		{
			auto attribs = obj["attributes"].toObject();

			// read brightness
			if (attribs.contains("brightness") && !attribs["brightness"].isNull())
			{
				lamp.orgState.brightness = attribs["brightness"].toInt(0);
			}
			else
			{
				lamp.orgState.brightness = -1;
			}

			// read color
			if (lamp.colorModel == HomeAssistantLamp::Mode::RGB &&
				attribs.contains("rgb_color") && attribs["rgb_color"].isArray())
			{
				lamp.orgState.color = attribs["rgb_color"].toArray();
			}
			else if (lamp.colorModel == HomeAssistantLamp::Mode::HSV &&
				attribs.contains("hs_color") && attribs["hs_color"].isArray())
			{
				lamp.orgState.color = attribs["hs_color"].toArray();
			}
			else
			{
				lamp.orgState.color = QJsonArray();
			}
		}

		QJsonDocument doc;
		doc.setArray(lamp.orgState.color);
		QString colorsToString = ((lamp.colorModel == HomeAssistantLamp::Mode::RGB) ? "rgb: " : "hs: " )+ doc.toJson(QJsonDocument::Compact);
		QString power = (lamp.orgState.isPoweredOn >= 0) ? ((lamp.orgState.isPoweredOn) ? "state: ON" : "state: OFF" ) : "";
		QString brightness = (lamp.orgState.isPoweredOn >= 0) ? (QString("brightness: %1" ).arg(lamp.orgState.brightness)) : "";
		QStringList message{ power, brightness, colorsToString };
				
		Info(_log, "Saving state of %s: %s", QSTRING_CSTR(lamp.name), QSTRING_CSTR(message.join(", ")));

	}
	return true;
}

void DriverNetHomeAssistant::restoreStates()
{
	QJsonDocument doc;
	for (auto& lamp : _haInstance.lamps)
	{
		QJsonObject row;

		row["entity_id"] = lamp.name;

		if (lamp.orgState.isPoweredOn < 0)
			continue;

		if (lamp.orgState.brightness >= 0)
		{
			row["brightness"] = lamp.orgState.brightness;
		}
		if (lamp.orgState.color.size() > 0)
		{
			if (lamp.colorModel == HomeAssistantLamp::Mode::RGB)
			{
				row["rgb_color"] = lamp.orgState.color;
			}
			else
			{
				row["hs_color"] = lamp.orgState.color;
			}
		}

		if (!row.isEmpty())
		{
			doc.setObject(row);
			QString message = doc.toJson(QJsonDocument::Compact);
			Info(_log, "Restoring state of %s: %s", QSTRING_CSTR(lamp.name), QSTRING_CSTR(message));
			_restApi->setBasePath(QString("/api/services/light/%1").arg((lamp.orgState.isPoweredOn) ? "turn_on" : "turn_off"));
			_restApi->post(message);
		}
	}
}

QJsonObject DriverNetHomeAssistant::discover(const QJsonObject& params)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

#ifdef ENABLE_BONJOUR
	std::shared_ptr<DiscoveryWrapper> bonInstance = _discoveryWrapper.lock();
	if (bonInstance != nullptr)
	{
		QList<DiscoveryRecord> recs;

		SAFE_CALL_0_RET(bonInstance.get(), getHomeAssistant, QList<DiscoveryRecord>, recs);

		for (DiscoveryRecord& r : recs)
		{
			QJsonObject newIp;
			newIp["value"] = QString("%1:8123").arg(r.address);
			newIp["name"] = QString("%1 (%2)").arg(newIp["value"].toString()).arg(r.hostName);
			deviceList.push_back(newIp);
		}
	}
#else
	Error(_log, "The Network Discovery Service was mysteriously disabled while the maintainer was compiling this version of HyperHDR");
#endif	

	devicesDiscovered.insert("devices", deviceList);
	Debug(_log, "devicesDiscovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

bool DriverNetHomeAssistant::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("home_assistant", "leds_group_2_network", DriverNetHomeAssistant::construct);
