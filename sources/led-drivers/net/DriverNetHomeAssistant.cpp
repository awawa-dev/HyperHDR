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
