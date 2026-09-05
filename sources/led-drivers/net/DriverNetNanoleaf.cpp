// Local-HyperHDR includes
#include <led-drivers/net/DriverNetNanoleaf.h>
#include <ssdp/SSDPDiscover.h>

// Qt includes
#include <QThread>
#include <QtEndian>

//std includes
#include <algorithm>
#include <cmath>

// Constants
namespace {
	const bool verbose = false;
	const bool verbose3 = false;

	// Configuration settings
	const char CONFIG_ADDRESS[] = "host";
	const char CONFIG_AUTH_TOKEN[] = "token";

	const char CONFIG_PANEL_ORDER_TOP_DOWN[] = "panelOrderTopDown";
	const char CONFIG_PANEL_ORDER_LEFT_RIGHT[] = "panelOrderLeftRight";
	const char CONFIG_PANEL_START_POS[] = "panelStartPos";
	const char CONFIG_STREAM_LEDS_PER_DATAGRAM[] = "streamLedsPerDatagram";
	const char CONFIG_STREAM_BATCH_GAP_MS[] = "streamBatchGapMs";

	// Panel configuration settings
	const char PANEL_LAYOUT[] = "layout";
	const char PANEL_NUM[] = "numPanels";
	const char PANEL_ID[] = "panelId";
	const char PANEL_POSITIONDATA[] = "positionData";
	const char PANEL_SHAPE_TYPE[] = "shapeType";
	const char PANEL_POS_X[] = "x";
	const char PANEL_POS_Y[] = "y";

	// List of State Information
	const char STATE_ON[] = "on";
	const char STATE_ONOFF_VALUE[] = "value";
	const char STATE_VALUE_TRUE[] = "true";
	const char STATE_VALUE_FALSE[] = "false";

	// Device Data elements
	const char DEV_DATA_NAME[] = "name";
	const char DEV_DATA_MODEL[] = "model";
	const char DEV_DATA_MANUFACTURER[] = "manufacturer";
	const char DEV_DATA_FIRMWAREVERSION[] = "firmwareVersion";
	const char DEV_DATA_NUM_LEDS[] = "numLEDs";

	// Nanoleaf Stream Control elements
	const char STREAM_CONTROL_PORT[] = "streamControlPort";
	const quint16 STREAM_CONTROL_DEFAULT_PORT = 60222;

	// Nanoleaf OpenAPI URLs
	const int API_DEFAULT_PORT = 16021;
	const char API_BASE_PATH[] = "/api/v1/%1/";
	const char API_ROOT[] = "";
	const char API_EXT_MODE_STRING_V2[] = "{\"write\" : {\"command\" : \"display\", \"animType\" : \"extControl\", \"extControlVersion\" : \"v2\"}}";
	const char API_STATE[] = "state";
	const char API_PANELLAYOUT[] = "panelLayout";
	const char API_EFFECT[] = "effects";
	const char API_LENGTH[] = "length";
	const char API_IDENTIFY[] = "identify";

	// Nanoleaf Control data stream (v2)
	const int STREAM_FRAME_PANEL_NUM_SIZE = 2;
	const int STREAM_FRAME_PANEL_INFO_SIZE = 8;
	// Ethernet MTU 1500 minus IP(20) and UDP(8). Batch only when a full frame would exceed this.
	const int STREAM_MAX_UDP_PAYLOAD = 1472;
	const int STREAM_MAX_LEDS_PER_DATAGRAM = (STREAM_MAX_UDP_PAYLOAD - STREAM_FRAME_PANEL_NUM_SIZE) / STREAM_FRAME_PANEL_INFO_SIZE;

	// Nanoleaf ssdp services
	const char SSDP_ID[] = "ssdp:all";
	const char SSDP_FILTER_HEADER[] = "ST";
	const char SSDP_NANOLEAF[] = "nanoleaf:nl*";
	const char SSDP_LIGHTPANELS[] = "nanoleaf_aurora:light";
} //End of constants

// Nanoleaf Panel Shapetypes
enum SHAPETYPES {
	TRIANGLE,
	RHYTM,
	SQUARE,
	CONTROL_SQUARE_PRIMARY,
	CONTROL_SQUARE_PASSIVE,
	POWER_SUPPLY,
};

// Nanoleaf external control versions
enum EXTCONTROLVERSIONS {
	EXTCTRLVER_V1 = 1,
	EXTCTRLVER_V2
};

DriverNetNanoleaf::DriverNetNanoleaf(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
	, _restApi(nullptr)
	, _apiPort(API_DEFAULT_PORT)
	, _topDown(true)
	, _leftRight(true)
	, _startPos(0)
	, _endPos(0)
	, _extControlVersion(EXTCTRLVER_V2)
	, _panelLedCount(0)
	, _isLightstrip(false)
	, _streamLedsPerDatagram(STREAM_MAX_LEDS_PER_DATAGRAM)
	, _streamBatchGapMs(0)
{
}

bool DriverNetNanoleaf::resolveApiEndpoint(const QString& host, QString& apiHost, int& apiPort) const
{
	if (host.isEmpty())
		return false;

	const QStringList addressparts = host.split(':', Qt::SkipEmptyParts);
	apiHost = addressparts[0];
	apiPort = (addressparts.size() > 1) ? addressparts[1].toInt() : API_DEFAULT_PORT;
	return !apiHost.isEmpty();
}

int DriverNetNanoleaf::queryNumLeds()
{
	if (_restApi == nullptr)
		return 0;

	_restApi->setPath(API_LENGTH);
	httpResponse response = _restApi->get();
	if (response.error())
	{
		Debug(_log, "GET /length failed: {:s}", (response.getErrorReason()));
		return 0;
	}

	return response.getBody().object()[DEV_DATA_NUM_LEDS].toInt(0);
}

bool DriverNetNanoleaf::applyConfiguredLedRange()
{
	const int configuredLedCount = this->getLedCount();
	_endPos = _startPos + configuredLedCount - 1;

	Debug(_log, "Sort Top>Down  : {:d}", _topDown);
	Debug(_log, "Sort Left>Right: {:d}", _leftRight);
	Debug(_log, "Start Panel Pos: {:d}", _startPos);
	Debug(_log, "End Panel Pos  : {:d}", _endPos);
	Debug(_log, "Hardware LEDs  : {:d}", _panelLedCount);

	if (_panelLedCount < configuredLedCount)
	{
		this->setInError(QString("Not enough panels [%1] for configured LEDs [%2] found!")
			.arg(_panelLedCount)
			.arg(configuredLedCount));
		return false;
	}

	if (_panelLedCount > configuredLedCount)
	{
		Info(_log, "{:s}: More panels [{:d}] than configured LEDs [{:d}].", (this->getActiveDeviceType()), _panelLedCount, configuredLedCount);
	}

	if (_endPos >= _panelLedCount)
	{
		this->setInError(QString("Start panel [%1] out of range. Start panel position can be max [%2] given [%3] panel available!")
			.arg(_startPos).arg(_panelLedCount - configuredLedCount).arg(_panelLedCount));
		return false;
	}

	return true;
}

bool DriverNetNanoleaf::loadLightstripIds(int ledCount)
{
	_isLightstrip = true;
	_panelIds.clear();
	_panelIds.reserve(ledCount);

	// Essentials OpenAPI: LED id == index in 0..N-1
	if (_leftRight)
	{
		for (int i = 0; i < ledCount; ++i)
			_panelIds.push_back(i);
	}
	else
	{
		for (int i = ledCount - 1; i >= 0; --i)
			_panelIds.push_back(i);
	}

	_panelLedCount = _panelIds.size();
	_devConfig["hardwareLedCount"] = _panelLedCount;
	return applyConfiguredLedRange();
}

bool DriverNetNanoleaf::loadPanelIds(const QJsonArray& positionData)
{
	_isLightstrip = false;
	std::map<int, std::map<int, int>> panelMap;

	for (const auto& value : positionData)
	{
		const QJsonObject panelObj = value.toObject();
		const int panelId = panelObj[PANEL_ID].toInt();
		const int panelX = panelObj[PANEL_POS_X].toInt();
		const int panelY = panelObj[PANEL_POS_Y].toInt();
		const int panelshapeType = panelObj[PANEL_SHAPE_TYPE].toInt();

		DebugIf(verbose, _log, "Panel [{:d}] ({:d},{:d}) - Type: [{:d}]", panelId, panelX, panelY, panelshapeType);

		if (panelshapeType != RHYTM)
		{
			panelMap[panelY][panelX] = panelId;
		}
		else
		{
			Info(_log, "Rhythm panel skipped.");
		}
	}

	for (auto posY = panelMap.crbegin(); posY != panelMap.crend(); ++posY)
	{
		if (_leftRight)
		{
			for (auto posX = posY->second.cbegin(); posX != posY->second.cend(); ++posX)
			{
				if (_topDown)
					_panelIds.push_back(posX->second);
				else
					_panelIds.push_front(posX->second);
			}
		}
		else
		{
			for (auto posX = posY->second.crbegin(); posX != posY->second.crend(); ++posX)
			{
				if (_topDown)
					_panelIds.push_back(posX->second);
				else
					_panelIds.push_front(posX->second);
			}
		}
	}

	_panelLedCount = _panelIds.size();
	_devConfig["hardwareLedCount"] = _panelLedCount;
	return applyConfiguredLedRange();
}

bool DriverNetNanoleaf::initLedsConfiguration()
{
	_panelIds.clear();
	_isLightstrip = false;

	_restApi->setPath(API_ROOT);
	httpResponse response = _restApi->get();
	if (response.error())
	{
		this->setInError(response.getErrorReason());
		return false;
	}

	const QJsonObject deviceInfo = response.getBody().object();
	_deviceModel = deviceInfo[DEV_DATA_MODEL].toString();
	_deviceFirmwareVersion = deviceInfo[DEV_DATA_FIRMWAREVERSION].toString();

	Debug(_log, "Name           : {:s}", (deviceInfo[DEV_DATA_NAME].toString()));
	Debug(_log, "Model          : {:s}", (_deviceModel));
	Debug(_log, "Manufacturer   : {:s}", (deviceInfo[DEV_DATA_MANUFACTURER].toString()));
	Debug(_log, "FirmwareVersion: {:s}", (_deviceFirmwareVersion));

	const QJsonObject jsonLayout = deviceInfo[API_PANELLAYOUT].toObject()[PANEL_LAYOUT].toObject();
	const QJsonArray positionData = jsonLayout[PANEL_POSITIONDATA].toArray();

	if (!positionData.isEmpty())
	{
		Debug(_log, "PanelsNum      : {:d}", jsonLayout[PANEL_NUM].toInt());
		return loadPanelIds(positionData);
	}

	// No panel layout: Essentials/lightstrip OpenAPI uses GET /length
	const int numLeds = queryNumLeds();
	if (numLeds <= 0)
	{
		this->setInError("Device has no panelLayout and GET /length returned no LEDs");
		return false;
	}

	Info(_log, "Nanoleaf lightstrip API (model {:s}): {:d} LEDs from /length", (_deviceModel), numLeds);
	return loadLightstripIds(numLeds);
}

bool DriverNetNanoleaf::init(QJsonObject deviceConfig)
{
	DebugIf(verbose, _log, "deviceConfig: [{:s}]", QString(QJsonDocument(_devConfig).toJson(QJsonDocument::Compact)).toUtf8().constData());

	if (!LedDevice::init(deviceConfig))
		return false;

	Debug(_log, "DeviceType   : {:s}", (this->getActiveDeviceType()));
	Debug(_log, "LedCount     : {:d}", this->getLedCount());
	Debug(_log, "RefreshTime  : {:d}", this->getRefreshTime());

	if (deviceConfig[CONFIG_PANEL_ORDER_TOP_DOWN].isString())
		_topDown = deviceConfig[CONFIG_PANEL_ORDER_TOP_DOWN].toString().toInt() == 0;
	else
		_topDown = deviceConfig[CONFIG_PANEL_ORDER_TOP_DOWN].toInt() == 0;

	if (deviceConfig[CONFIG_PANEL_ORDER_LEFT_RIGHT].isString())
		_leftRight = deviceConfig[CONFIG_PANEL_ORDER_LEFT_RIGHT].toString().toInt() == 0;
	else
		_leftRight = deviceConfig[CONFIG_PANEL_ORDER_LEFT_RIGHT].toInt() == 0;

	_startPos = deviceConfig[CONFIG_PANEL_START_POS].toInt(0);

	_hostname = deviceConfig[CONFIG_ADDRESS].toString();
	_apiPort = API_DEFAULT_PORT;
	_authToken = deviceConfig[CONFIG_AUTH_TOKEN].toString();

	if (_hostname.isEmpty())
	{
		this->setInError("No target hostname nor IP defined");
		return false;
	}

	if (!initRestAPI(_hostname, _apiPort, _authToken) || !initLedsConfiguration())
		return false;

	if (_isLightstrip)
	{
		// Default: one datagram with every LED (matches HyperHDR full-frame output).
		// Override streamLedsPerDatagram if a controller cannot take a full frame.
		const int defaultLedsPerDatagram = std::max(1, _panelLedCount);
		_streamLedsPerDatagram = std::max(1, deviceConfig[CONFIG_STREAM_LEDS_PER_DATAGRAM].toInt(defaultLedsPerDatagram));
		_streamBatchGapMs = std::max(0, deviceConfig[CONFIG_STREAM_BATCH_GAP_MS].toInt(0));
		Info(_log, "Lightstrip stream: {:d} LEDs/datagram, {:d}ms batch gap (refresh follows smoothing)",
			_streamLedsPerDatagram, _streamBatchGapMs);
	}

	_devConfig["host"] = _hostname;
	_devConfig["port"] = STREAM_CONTROL_DEFAULT_PORT;

	const bool isInitOK = ProviderUdp::init(_devConfig);
	Debug(_log, "Hostname/IP  : {:s}", (_hostname));
	Debug(_log, "Port         : {:d}", _port);
	return isInitOK;
}

bool DriverNetNanoleaf::initRestAPI(const QString& hostname, int port, const QString& token)
{
	if (_restApi == nullptr)
		_restApi = std::make_unique<ProviderRestApi>(hostname, port);
	else
		_restApi->updateHost(hostname, port);

	_restApi->setBasePath(QString(API_BASE_PATH).arg(token));
	return true;
}

int DriverNetNanoleaf::open()
{
	_isDeviceReady = false;

	const QJsonObject streamInfo = changeToExternalControlMode().object();
	if (streamInfo.contains(STREAM_CONTROL_PORT))
	{
		const int streamPort = streamInfo[STREAM_CONTROL_PORT].toInt();
		if (streamPort > 0)
			_port = static_cast<quint16>(streamPort);
	}

	if (ProviderUdp::open() != 0)
		return -1;

	applyStreamMasterBrightness();
	_isDeviceReady = true;
	return 0;
}

QJsonDocument DriverNetNanoleaf::changeToExternalControlMode()
{
	Debug(_log, "Set Nanoleaf to External Control (UDP) streaming mode");

	if (_restApi == nullptr)
		return {};

	_extControlVersion = EXTCTRLVER_V2;
	_restApi->setPath(API_EFFECT);
	return _restApi->put(API_EXT_MODE_STRING_V2).getBody();
}

bool DriverNetNanoleaf::applyStreamMasterBrightness()
{
	if (_restApi == nullptr)
		return false;

	// Essentials global brightness multiplies extControl / screen-mirror output.
	_restApi->setPath(API_STATE);
	httpResponse response = _restApi->put(QString("{\"brightness\":{\"value\":100}}"));
	if (response.error())
	{
		Warning(_log, "Could not set Nanoleaf brightness to 100: {:s}", (response.getErrorReason()));
		return false;
	}

	Info(_log, "Nanoleaf stream master brightness set to 100");
	return true;
}

ColorRgb DriverNetNanoleaf::candyColor(const ColorRgb& color) const
{
	if (color.red == 0 && color.green == 0 && color.blue == 0)
		return color;

	uint16_t hue = 0;
	uint8_t sat = 0;
	uint8_t val = 0;
	ColorRgb::rgb2hsv(color.red, color.green, color.blue, hue, sat, val);

	// Neon saturation: keep hue, pull S toward 255.
	sat = static_cast<uint8_t>(sat + static_cast<uint16_t>(255 - sat) * 3 / 5);

	// Lift midtones so TV-content greys still punch on the strip.
	const float v = val / 255.0f;
	const int lifted = static_cast<int>(std::lround(255.0f * std::pow(v, 0.62f) * 1.12f));
	val = static_cast<uint8_t>(std::clamp(lifted, 0, 255));

	ColorRgb out;
	ColorRgb::hsv2rgb(hue, sat, val, out.red, out.green, out.blue);
	return out;
}

QJsonObject DriverNetNanoleaf::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

	SSDPDiscover discover;
	discover.setSearchFilter(QString("%1|%2").arg(SSDP_NANOLEAF, SSDP_LIGHTPANELS), SSDP_FILTER_HEADER);

	QJsonArray deviceList;
	if (discover.discoverServices(SSDP_ID) > 0)
		deviceList = discover.getServicesDiscoveredJson();

	devicesDiscovered.insert("devices", deviceList);
	Debug(_log, "devicesDiscovered: [{:s}]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());
	return devicesDiscovered;
}

void DriverNetNanoleaf::identify(const QJsonObject& params)
{
	Debug(_log, "params: [{:s}]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());

	QString apiHost;
	int apiPort = API_DEFAULT_PORT;
	if (!resolveApiEndpoint(params["host"].toString(), apiHost, apiPort))
		return;

	initRestAPI(apiHost, apiPort, params["token"].toString());
	_restApi->setPath(API_IDENTIFY);

	httpResponse response = _restApi->put();
	if (response.error())
	{
		Warning(_log, "{:s} identification failed with error: '{:s}'", (_activeDeviceType), (response.getErrorReason()));
	}
}

QJsonObject DriverNetNanoleaf::getProperties(const QJsonObject& params)
{
	Debug(_log, "params: [{:s}]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());
	QJsonObject properties;

	QString apiHost;
	int apiPort = API_DEFAULT_PORT;
	if (!resolveApiEndpoint(params["host"].toString(), apiHost, apiPort))
		return properties;

	initRestAPI(apiHost, apiPort, params["token"].toString());
	_restApi->setPath(params["filter"].toString());

	httpResponse response = _restApi->get();
	if (response.error())
	{
		Warning(_log, "{:s} get properties failed with error: '{:s}'", (_activeDeviceType), (response.getErrorReason()));
	}

	QJsonObject props = response.getBody().object();
	const QJsonArray positionData = props[API_PANELLAYOUT].toObject()[PANEL_LAYOUT].toObject()[PANEL_POSITIONDATA].toArray();
	if (positionData.isEmpty())
	{
		const int numLeds = queryNumLeds();
		if (numLeds > 0)
		{
			props[DEV_DATA_NUM_LEDS] = numLeds;
			props["hardwareLedCount"] = numLeds;
		}
	}

	properties.insert("properties", props);
	Debug(_log, "properties: [{:s}]", QString(QJsonDocument(properties).toJson(QJsonDocument::Compact)).toUtf8().constData());
	return properties;
}

QString DriverNetNanoleaf::getOnOffRequest(bool isOn) const
{
	if (isOn)
		return QString("{\"%1\":{\"%2\":%3},\"brightness\":{\"value\":100}}").arg(STATE_ON, STATE_ONOFF_VALUE, STATE_VALUE_TRUE);

	return QString("{\"%1\":{\"%2\":%3}}").arg(STATE_ON, STATE_ONOFF_VALUE, STATE_VALUE_FALSE);
}

bool DriverNetNanoleaf::powerOn()
{
	if (_isDeviceReady)
	{
		changeToExternalControlMode();
		_restApi->setPath(API_STATE);
		_restApi->put(getOnOffRequest(true));
	}
	return true;
}

bool DriverNetNanoleaf::powerOff()
{
	if (_isDeviceReady)
	{
		_restApi->setPath(API_STATE);
		_restApi->put(getOnOffRequest(false));
	}
	return true;
}

int DriverNetNanoleaf::writeStreamBatch(const std::vector<ColorRgb>& ledValues, int startIndex, int count, int& ledCounter)
{
	QByteArray udpbuffer;
	udpbuffer.resize(STREAM_FRAME_PANEL_NUM_SIZE + count * STREAM_FRAME_PANEL_INFO_SIZE);

	int i = 0;
	qToBigEndian<quint16>(static_cast<quint16>(count), udpbuffer.data() + i);
	i += 2;

	// Panels keep the original 100ms transition; lightstrips use 0 for video sync
	const quint16 transitionTime = _isLightstrip ? 0 : 1;

	for (int j = 0; j < count; ++j)
	{
		const int panelCounter = startIndex + j;
		ColorRgb color = ColorRgb::BLACK;

		if (panelCounter >= _startPos && panelCounter <= _endPos && ledCounter < static_cast<int>(ledValues.size()))
		{
			color = ledValues[static_cast<size_t>(ledCounter)];
			if (_isLightstrip)
				color = candyColor(color);
			++ledCounter;
		}

		qToBigEndian<quint16>(static_cast<quint16>(_panelIds[panelCounter]), udpbuffer.data() + i);
		i += 2;
		udpbuffer[i++] = static_cast<char>(color.red);
		udpbuffer[i++] = static_cast<char>(color.green);
		udpbuffer[i++] = static_cast<char>(color.blue);
		udpbuffer[i++] = 0;
		qToBigEndian<quint16>(transitionTime, udpbuffer.data() + i);
		i += 2;
	}

	return writeBytes(udpbuffer);
}

int DriverNetNanoleaf::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	// v2 stream frame: nLeds(2B) + [id(2B) RGBW(4B) transition(2B)] * n
	// Write rate is owned by HyperHDR smoothing / LedDevice refresh, not this driver.

	const int batchSize = _isLightstrip ? _streamLedsPerDatagram : _panelLedCount;
	int retVal = 0;
	int ledCounter = 0;

	for (int index = 0; index < _panelLedCount; index += batchSize)
	{
		if (index > 0 && _streamBatchGapMs > 0)
			QThread::msleep(static_cast<unsigned long>(_streamBatchGapMs));

		const int count = std::min(batchSize, _panelLedCount - index);
		retVal = writeStreamBatch(ledValues, index, count, ledCounter);
		if (retVal < 0)
			return retVal;
	}

	return retVal;
}

LedDevice* DriverNetNanoleaf::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetNanoleaf(deviceConfig);
}

bool DriverNetNanoleaf::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("nanoleaf", "leds_group_2_network", DriverNetNanoleaf::construct);
