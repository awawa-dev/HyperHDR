// Local-HyperHDR includes
#include <led-drivers/net/DriverNetWled.h>
#include <HyperhdrConfig.h>
#include <QString>
#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>
#endif

DriverNetWled::DriverNetWled(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
	, _restApi(nullptr)
	, _apiPort(80)
	, _warlsStreamPort(21324)
	, _overrideBrightness(true)
	, _brightnessLevel(255)
	, _restoreConfig(false)
{
}

DriverNetWled::~DriverNetWled()
{
	delete _restApi;
	_restApi = nullptr;
}

LedDevice* DriverNetWled::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetWled(deviceConfig);
}

bool DriverNetWled::init(const QJsonObject& deviceConfig)
{	
	bool isInitOK = false;

	Debug(_log, "Initializing WLED");

	_configBackup = QJsonDocument();

	// Initialise LedDevice sub-class, ProviderUdp::init will be executed later, if connectivity is defined
	if (LedDevice::init(deviceConfig))
	{
		// Initialise LedDevice configuration and execution environment
		int configuredLedCount = this->getLedCount();
		Debug(_log, "DeviceType     : %s", QSTRING_CSTR(this->getActiveDeviceType()));
		Debug(_log, "LedCount       : %d", configuredLedCount);

		_overrideBrightness = deviceConfig["brightnessMax"].toBool(true);
		Debug(_log, "Override brightness : %s", (_overrideBrightness) ? "true" : "false");
		
		_brightnessLevel = deviceConfig["brightnessMaxLevel"].toInt(255);
		Debug(_log, "Set brightness level: %i", _brightnessLevel);

		_restoreConfig = deviceConfig["restoreOriginalState"].toBool(false);
		Debug(_log, "Restore WLED   : %s", (_restoreConfig) ? "true" : "false");

		_maxRetry = deviceConfig["maxRetry"].toInt(60);
		Debug(_log, "Max retry      : %d", _maxRetry);

		//Set hostname as per configuration
		QString address = deviceConfig["host"].toString();

		//If host not configured the init fails
		if (address.isEmpty())
		{
			this->setInError("No target hostname nor IP defined");
			return false;
		}
		else
		{
			QStringList addressparts = address.split(':', Qt::SkipEmptyParts);
			_hostname = addressparts[0];
			if (addressparts.size() > 1)
			{
				_apiPort = addressparts[1].toInt();
			}

			if (initRestAPI(_hostname, _apiPort))
			{
				// Update configuration with hostname without port
				_devConfig["host"] = _hostname;
				_devConfig["port"] = _warlsStreamPort;

				isInitOK = ProviderUdp::init(_devConfig);
				Debug(_log, "Hostname/IP  : %s", QSTRING_CSTR(_hostname));
				Debug(_log, "Port         : %d", _port);
			}
		}
	}
	Debug(_log, "[%d]", isInitOK);
	return isInitOK;
}

bool DriverNetWled::initRestAPI(const QString& hostname, int port)
{
	Debug(_log, "");
	bool isInitOK = false;

	if (_restApi == nullptr)
	{
		_restApi = new ProviderRestApi(hostname, port);
		_restApi->setBasePath("/json");

		isInitOK = true;
	}

	Debug(_log, "[%d]", isInitOK);
	return isInitOK;
}

QString DriverNetWled::getOnOffRequest(bool isOn) const
{
	if (!isOn && _restoreConfig && !_configBackup.isEmpty())
	{
		return QString(_configBackup.toJson(QJsonDocument::Compact));
	}
	else
	{
		QString state = isOn ? "true" : "false";
		QString bri = (_overrideBrightness && isOn) ? QString(",\"bri\":%1").arg(_brightnessLevel) : "";
		return QString("{\"on\":%1,\"live\":%1%2}").arg(state).arg(bri);
	}
}

bool DriverNetWled::powerOn()
{
	Debug(_log, "");

	_restApi->setPath("");
	httpResponse response = _restApi->get();

	auto wledConfig = response.getBody();
	if (wledConfig.isEmpty())
	{
		response.setError(true);
		response.setErrorReason("Empty WLED config");
	}

	if (!response.error())
	{
		if (wledConfig.isObject())
		{
			QJsonObject mainConfig = wledConfig.object();
			QJsonObject infoConfig = mainConfig["info"].toObject();
			QJsonObject ledsConfig = infoConfig["leds"].toObject();
			QJsonObject stateConfig = mainConfig["state"].toObject();
			QJsonObject wifiConfig = infoConfig["wifi"].toObject();

			if (_restoreConfig)
			{
				stateConfig["live"] = false;
				_configBackup.setObject(stateConfig);
			}

			uint ledsNumber = ledsConfig["count"].toInt(0);
			int powerLimiter = ledsConfig["maxpwr"].toInt(0);
			int quality = wifiConfig["signal"].toInt(0);

			_warlsStreamPort = infoConfig["udpport"].toInt(_warlsStreamPort);

			QString infoMessage = QString("WLED info => wifi quality: %1, wifi channel: %2, leds: %3, arch: %4, ver: %5, uptime: %6s, port: %7, power limit: %8mA")
				.arg(QString::number(quality) + ((quality < 80) ?  + "% (LOW)" : "%") ).arg(wifiConfig["channel"].toInt()).arg(ledsNumber)
				.arg(infoConfig["arch"].toString()).arg(infoConfig["ver"].toString())
				.arg(infoConfig["uptime"].toInt()).arg(_warlsStreamPort).arg(powerLimiter);

			if (quality < 80 || powerLimiter > 0 || _ledCount != ledsNumber)
				Warning(_log, "%s", QSTRING_CSTR(infoMessage));
			else
				Info(_log, "%s", QSTRING_CSTR(infoMessage));
			
			if (powerLimiter > 0)
				Error(_log, "Serious warning: the power limiter in WLED is set which may lead to unexpected side effects. Use the right cabling & power supply with the appropriate power, not this half-measure.");

			if (_ledCount != ledsNumber)
				Warning(_log, "The number of LEDs defined in HyperHDR (%i) is different from that defined in WLED (%i)", _ledCount, ledsNumber);
			
			_customInfo = QString("  %1%").arg(quality);

			ProviderUdp::setPort(_warlsStreamPort);
		}
		else
			Warning(_log, "Could not read WLED config");

		_restApi->setPath("/state");
		response = _restApi->put(getOnOffRequest(true));
	}

	if (response.error())
	{
		this->setInError(response.error() ? response.getErrorReason() : "Empty WLED config");
		setupRetry(1500);
		return false;
	}

	return true;
}

bool DriverNetWled::powerOff()
{
	Debug(_log, "");
	bool off = true;

	_customInfo = "";

	if (_isDeviceReady)
	{
		// Write a final "Black" to have a defined outcome
		writeBlack();

		//Power-off the WLED device physically
		_restApi->setPath("/state");
		httpResponse response = _restApi->put(getOnOffRequest(false));
		if (response.error())
		{
			this->setInError(response.getErrorReason());
			off = false;
		}
	}
	return off;
}

QJsonObject DriverNetWled::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	
#ifdef ENABLE_BONJOUR
	std::shared_ptr<DiscoveryWrapper> bonInstance = _discoveryWrapper.lock();
	if (bonInstance != nullptr)
	{
		QList<DiscoveryRecord> recs;

		SAFE_CALL_0_RET(bonInstance.get(), getWLED, QList<DiscoveryRecord>, recs);

		for (DiscoveryRecord& r : recs)
		{
			QJsonObject newIp;
			newIp["value"] = QString("%1").arg(r.address);
			newIp["name"] = QString("%1 (%2)").arg(newIp["value"].toString()).arg(r.hostName);
			deviceList.push_back(newIp);
		}
	}
#else
	Error(_log, "The Network Discovery Service was mysteriously disabled while the maintenair was compiling this version of HyperHDR");
#endif	

	devicesDiscovered.insert("devices", deviceList);
	Debug(_log, "devicesDiscovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

int DriverNetWled::write(const std::vector<ColorRgb>& ledValues)
{
	if (ledValues.size() != _ledCount)
	{
		setLedCount(static_cast<int>(ledValues.size()));
		return 0;
	}
	else if (ledValues.size() <= 490)
	{
		int wledSize = _ledRGBCount + 2;
		std::vector<uint8_t> wledData(wledSize, 0);
		wledData[0] = 2;
		wledData[1] = 255;
		memcpy(wledData.data() + 2, ledValues.data(), _ledRGBCount);

		return writeBytes(wledSize, wledData.data());
	}
	else
	{
		long long offset = 0;
		const uint8_t* start = reinterpret_cast<const uint8_t*>(ledValues.data());
		const uint8_t* end = reinterpret_cast<const uint8_t*>(ledValues.data()) + _ledRGBCount;

		while (start < end)
		{
			auto realSize = std::min(static_cast<long int>(end - start), static_cast<long int>(489 * sizeof(ColorRgb)));
			std::vector<uint8_t> wledData(realSize + 4, 0);
			wledData[0] = 4;
			wledData[1] = 255;
			wledData[2] = ((offset >> 8) & 0xff);
			wledData[3] = (offset & 0xff);
			memcpy(wledData.data() + 4, start, realSize);
			start += realSize;
			offset += realSize / sizeof(ColorRgb);
			writeBytes(static_cast<int>(wledData.size()), wledData.data());
		}

		return _ledRGBCount;
	}
}

bool DriverNetWled::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("wled", "leds_group_2_network", DriverNetWled::construct);
