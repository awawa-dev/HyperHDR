// Local-HyperHDR includes
#include "LedDeviceWled.h"
#include <utils/QStringUtils.h>
#include <HyperhdrConfig.h>
#include <QString>
#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>
#endif

// Constants
namespace {

	// Configuration settings
	const char CONFIG_ADDRESS[] = "host";

	// UDP elements
	const quint16 STREAM_DEFAULT_PORT = 19446;

	// WLED JSON-API elements
	const int API_DEFAULT_PORT = -1; //Use default port per communication scheme

	const char API_BASE_PATH[] = "/json/";
	//const char API_PATH_INFO[] = "info";
	const char API_PATH_STATE[] = "state";

	// List of State Information
	const char STATE_VALUE_TRUE[] = "true";
	const char STATE_VALUE_FALSE[] = "false";
} //End of constants

LedDeviceWled::LedDeviceWled(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
	, _restApi(nullptr)
	, _apiPort(API_DEFAULT_PORT)
	, _overrideBrightness(true)
	, _brightnessLevel(255)
	, _restoreConfig(false)
{
}

LedDeviceWled::~LedDeviceWled()
{
	delete _restApi;
	_restApi = nullptr;
}

LedDevice* LedDeviceWled::construct(const QJsonObject& deviceConfig)
{
	return new LedDeviceWled(deviceConfig);
}

bool LedDeviceWled::init(const QJsonObject& deviceConfig)
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
		QString address = deviceConfig[CONFIG_ADDRESS].toString();

		//If host not configured the init fails
		if (address.isEmpty())
		{
			this->setInError("No target hostname nor IP defined");
			return false;
		}
		else
		{
			QStringList addressparts = QStringUtils::SPLITTER(address, ':');
			_hostname = addressparts[0];
			if (addressparts.size() > 1)
			{
				_apiPort = addressparts[1].toInt();
			}
			else
			{
				_apiPort = API_DEFAULT_PORT;
			}

			if (initRestAPI(_hostname, _apiPort))
			{
				// Update configuration with hostname without port
				_devConfig["host"] = _hostname;
				_devConfig["port"] = STREAM_DEFAULT_PORT;

				isInitOK = ProviderUdp::init(_devConfig);
				Debug(_log, "Hostname/IP  : %s", QSTRING_CSTR(_hostname));
				Debug(_log, "Port         : %d", _port);
			}
		}
	}
	Debug(_log, "[%d]", isInitOK);
	return isInitOK;
}

bool LedDeviceWled::initRestAPI(const QString& hostname, int port)
{
	Debug(_log, "");
	bool isInitOK = false;

	if (_restApi == nullptr)
	{
		_restApi = new ProviderRestApi(hostname, port);
		_restApi->setBasePath(API_BASE_PATH);

		isInitOK = true;
	}

	Debug(_log, "[%d]", isInitOK);
	return isInitOK;
}

QString LedDeviceWled::getOnOffRequest(bool isOn) const
{
	if (!isOn && _restoreConfig && !_configBackup.isEmpty())
	{
		return QString(_configBackup.toJson(QJsonDocument::Compact));
	}
	else
	{
		QString state = isOn ? STATE_VALUE_TRUE : STATE_VALUE_FALSE;
		QString bri = (_overrideBrightness && isOn) ? QString(",\"bri\":%1").arg(_brightnessLevel) : "";
		return QString("{\"on\":%1,\"live\":%1%2}").arg(state).arg(bri);
	}
}

bool LedDeviceWled::powerOn()
{
	Debug(_log, "");
	bool on = false;

	if (!_retryMode)
	{
		//Power-on WLED device
		_restApi->setPath(API_PATH_STATE);

		bool readConfig = _restoreConfig && _configBackup.isEmpty();
		httpResponse response = (readConfig) ? _restApi->get() : _restApi->put(getOnOffRequest(true));

		if (readConfig && !response.error())
		{
			_configBackup = response.getBody();

			if (_configBackup.isObject())
			{
				auto copy = _configBackup.object();
				copy["live"] = false;
				_configBackup.setObject(copy);
			}

			response = _restApi->put(getOnOffRequest(true));
		}

		if (response.error())
		{
			this->setInError(response.getErrorReason());			

			// power on simultaneously with Rpi causes timeout			
			if (_maxRetry > 0 && response.error())
			{
				if (_currentRetry <= 0)
					_currentRetry = _maxRetry + 1;

				_currentRetry--;

				if (_currentRetry > 0)
					Warning(_log, "The WLED device is not ready... will try to reconnect (try %i/%i).", (_maxRetry - _currentRetry + 1), _maxRetry);
				else
					Error(_log, "The WLED device is not ready... give up.");

				if (_currentRetry > 0 && !_signalTerminate)
				{
					_retryMode = true;
					QTimer::singleShot(1000, [this]() { _retryMode = false; if (_currentRetry > 0 && !_signalTerminate) enableDevice(true);  });
				}
			}
		}
		else
			on = true;
	}
	return on;
}

bool LedDeviceWled::powerOff()
{
	Debug(_log, "");
	bool off = true;

	_currentRetry = 0;
	_retryMode = false;

	if (_isDeviceReady)
	{
		// Write a final "Black" to have a defined outcome
		writeBlack();

		//Power-off the WLED device physically
		_restApi->setPath(API_PATH_STATE);
		httpResponse response = _restApi->put(getOnOffRequest(false));
		if (response.error())
		{
			this->setInError(response.getErrorReason());
			off = false;
		}
	}
	return off;
}

QJsonObject LedDeviceWled::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	
#ifdef ENABLE_BONJOUR
	auto bonInstance = DiscoveryWrapper::getInstance();
	if (bonInstance != nullptr)
	{
		QList<DiscoveryRecord> recs;

		SAFE_CALL_0_RET(bonInstance, getWLED, QList<DiscoveryRecord>, recs);

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

QJsonObject LedDeviceWled::getProperties(const QJsonObject& params)
{
	Debug(_log, "params: [%s]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());
	QJsonObject properties;

	// Get Nanoleaf device properties
	QString host = params["host"].toString("");
	if (!host.isEmpty())
	{
		QString filter = params["filter"].toString("");

		// Resolve hostname and port (or use default API port)
		QStringList addressparts = QStringUtils::SPLITTER(host, ':');
		QString apiHost = addressparts[0];
		int apiPort;

		if (addressparts.size() > 1)
		{
			apiPort = addressparts[1].toInt();
		}
		else
		{
			apiPort = API_DEFAULT_PORT;
		}

		initRestAPI(apiHost, apiPort);
		_restApi->setPath(filter);

		httpResponse response = _restApi->get();
		if (response.error())
		{
			Warning(_log, "%s get properties failed with error: '%s'", QSTRING_CSTR(_activeDeviceType), QSTRING_CSTR(response.getErrorReason()));
		}

		properties.insert("properties", response.getBody().object());

		Debug(_log, "properties: [%s]", QString(QJsonDocument(properties).toJson(QJsonDocument::Compact)).toUtf8().constData());
	}
	return properties;
}


int LedDeviceWled::write(const std::vector<ColorRgb>& ledValues)
{
	const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(ledValues.data());

	if (ledValues.size() != _ledCount)
		setLedCount(static_cast<int>(ledValues.size()));

	return writeBytes(_ledRGBCount, dataPtr);
}
