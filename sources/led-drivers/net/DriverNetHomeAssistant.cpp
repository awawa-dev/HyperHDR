#include <led-drivers/net/DriverNetHomeAssistant.h>

#include <HyperhdrConfig.h>
#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>
#endif

DriverNetHomeAssistant::DriverNetHomeAssistant(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
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
		_haInstance.restoreOriginalState = deviceConfig["restoreOriginalState"].toBool(false);
		_maxRetry = deviceConfig["maxRetry"].toInt(60);

		
		Debug(_log, "HomeAssistantHost    : %s", QSTRING_CSTR(_haInstance.homeAssistantHost));
		Debug(_log, "RestoreOriginalState : %s", _haInstance.restoreOriginalState ? "yes" : "no");
		Debug(_log, "Max retry            : %d", _maxRetry);

		auto arr = deviceConfig["lamps"].toArray();

		for (const auto&& lamp : arr)
			if (lamp.isObject())
			{
				HomeAssistantLamp hl;
				auto lampObj = lamp.toObject();
				hl.name = lampObj["name"].toString();
				hl.colorModel = static_cast<HomeAssistantLamp::Mode>(lampObj["colorModel"].toInt(0));				
				Debug(_log, "New lamp (%s)       : %s", (hl.colorModel == 0) ? "RGB" : "HSV", QSTRING_CSTR(hl.name));
				_haInstance.lamps.push_back(hl);
			}

		if (_haInstance.homeAssistantHost.length() > 0 && _haInstance.longLivedAccessToken.length() > 0 && arr.size() > 0)
		{
			isInitOK = true;
		}
	}
	return isInitOK;
}

int DriverNetHomeAssistant::write(const std::vector<ColorRgb>& ledValues)
{
	return 0;
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
