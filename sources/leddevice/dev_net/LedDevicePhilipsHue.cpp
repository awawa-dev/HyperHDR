// Local-HyperHDR includes
#include "LedDevicePhilipsHue.h"
#include <utils/QStringUtils.h>
#include <HyperhdrConfig.h>
#ifdef ENABLE_BONJOUR
	#include <bonjour/bonjourbrowserwrapper.h>
#endif
#include <ssdp/SSDPDiscover.h>


#include <QDateTime>
#include <chrono>

// Constants
namespace {

	bool verbose = false;

	// Configuration settings
	const char CONFIG_ADDRESS[] = "output";
	//const char CONFIG_PORT[] = "port";
	const char CONFIG_USERNAME[] = "username";
	const char CONFIG_CLIENTKEY[] = "clientkey";
	const char CONFIG_BRIGHTNESSFACTOR[] = "brightnessFactor";
	const char CONFIG_TRANSITIONTIME[] = "transitiontime";
	const char CONFIG_BLACK_LIGHTS_TIMEOUT[] = "blackLightsTimeout";
	const char CONFIG_ON_OFF_BLACK[] = "switchOffOnBlack";
	const char CONFIG_RESTORE_STATE[] = "restoreOriginalState";
	const char CONFIG_LIGHTIDS[] = "lightIds";
	const char CONFIG_USE_HUE_ENTERTAINMENT_API[] = "useEntertainmentAPI";
	const char CONFIG_GROUPID[] = "groupId";

	const char CONFIG_VERBOSE[] = "verbose";

	// Device Data elements
	const char DEV_DATA_BRIDGEID[] = "bridgeid";
	const char DEV_DATA_MODEL[] = "modelid";
	const char DEV_DATA_NAME[] = "name";
	//const char DEV_DATA_MANUFACTURER[] = "manufacturer";
	const char DEV_DATA_FIRMWAREVERSION[] = "swversion";
	const char DEV_DATA_APIVERSION[] = "apiversion";

	// Philips Hue OpenAPI URLs
	const int API_DEFAULT_PORT = -1; //Use default port per communication scheme
	const char API_BASE_PATH[] = "/api/%1/";
	const char API_ROOT[] = "";
	const char API_STATE[] = "state";
	const char API_CONFIG[] = "config";
	const char API_LIGHTS[] = "lights";
	const char API_GROUPS[] = "groups";

	// List of Group / Stream Information
	const char API_GROUP_NAME[] = "name";
	const char API_GROUP_TYPE[] = "type";
	const char API_GROUP_TYPE_ENTERTAINMENT[] = "Entertainment";
	const char API_STREAM[] = "stream";
	const char API_STREAM_ACTIVE[] = "active";
	const char API_STREAM_ACTIVE_VALUE_TRUE[] = "true";
	const char API_STREAM_ACTIVE_VALUE_FALSE[] = "false";
	const char API_STREAM_OWNER[] = "owner";
	const char API_STREAM_RESPONSE_FORMAT[] = "/%1/%2/%3/%4";

	// List of resources
	const char API_XY_COORDINATES[] = "xy";
	const char API_BRIGHTNESS[] = "bri";
	//const char API_SATURATION[] = "sat";
	const char API_TRANSITIONTIME[] = "transitiontime";
	const char API_MODEID[] = "modelid";

	// List of State Information
	const char API_STATE_ON[] = "on";
	const char API_STATE_VALUE_TRUE[] = "true";
	const char API_STATE_VALUE_FALSE[] = "false";

	// List of Error Information
	const char API_ERROR[] = "error";
	const char API_ERROR_ADDRESS[] = "address";
	const char API_ERROR_DESCRIPTION[] = "description";
	const char API_ERROR_TYPE[] = "type";

	// List of Success Information
	const char API_SUCCESS[] = "success";

	// Phlips Hue ssdp services
	const char SSDP_ID[] = "upnp:rootdevice";
	const char SSDP_FILTER[] = "(.*)IpBridge(.*)";
	const char SSDP_FILTER_HEADER[] = "SERVER";

	// DTLS Connection / SSL / Cipher Suite
	const char API_SSL_SERVER_NAME[] = "Hue";
	const char API_SSL_SEED_CUSTOM[] = "dtls_client";
	const int API_SSL_SERVER_PORT = 2100;
	const int STREAM_SSL_HANDSHAKE_ATTEMPTS = 5;
	constexpr std::chrono::milliseconds STREAM_REFRESH_TIME{ 20 };
	const int SSL_CIPHERSUITES[2] = { MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256, 0 };

} //End of constants

bool operator ==(const CiColor& p1, const CiColor& p2)
{
	return ((p1.x == p2.x) && (p1.y == p2.y) && (p1.bri == p2.bri));
}

bool operator != (const CiColor& p1, const CiColor& p2)
{
	return !(p1 == p2);
}

CiColor CiColor::rgbToCiColor(double red, double green, double blue, const CiColorTriangle& colorSpace, bool candyGamma)
{
	double cx;
	double cy;
	double bri;

	if (red + green + blue > 0)
	{
		// Apply gamma correction.
		double r = red;
		double g = green;
		double b = blue;

		if (candyGamma)
		{
			r = (red > 0.04045) ? pow((red + 0.055) / (1.0 + 0.055), 2.4) : (red / 12.92);
			g = (green > 0.04045) ? pow((green + 0.055) / (1.0 + 0.055), 2.4) : (green / 12.92);
			b = (blue > 0.04045) ? pow((blue + 0.055) / (1.0 + 0.055), 2.4) : (blue / 12.92);
		}

		// Convert to XYZ space.
		double X = r * 0.664511 + g * 0.154324 + b * 0.162028;
		double Y = r * 0.283881 + g * 0.668433 + b * 0.047685;
		double Z = r * 0.000088 + g * 0.072310 + b * 0.986039;

		cx = X / (X + Y + Z);
		cy = Y / (X + Y + Z);

		// RGB to HSV/B Conversion before gamma correction V/B for brightness, not Y from XYZ Space.
		// bri = std::max(std::max(red, green), blue);
		// RGB to HSV/B Conversion after gamma correction V/B for brightness, not Y from XYZ Space.
		bri = std::max(r, std::max(g, b));
	}
	else
	{
		cx = 0.0;
		cy = 0.0;
		bri = 0.0;
	}

	if (std::isnan(cx))
	{
		cx = 0.0;
	}
	if (std::isnan(cy))
	{
		cy = 0.0;
	}
	if (std::isnan(bri))
	{
		bri = 0.0;
	}

	CiColor xy = { cx, cy, bri };

	if ((red + green + blue) > 0)
	{
		// Check if the given XY value is within the color reach of our lamps.
		if (!isPointInLampsReach(xy, colorSpace))
		{
			// It seems the color is out of reach let's find the closes color we can produce with our lamp and send this XY value out.
			XYColor pAB = getClosestPointToPoint(colorSpace.red, colorSpace.green, xy);
			XYColor pAC = getClosestPointToPoint(colorSpace.blue, colorSpace.red, xy);
			XYColor pBC = getClosestPointToPoint(colorSpace.green, colorSpace.blue, xy);
			// Get the distances per point and see which point is closer to our Point.
			double dAB = getDistanceBetweenTwoPoints(xy, pAB);
			double dAC = getDistanceBetweenTwoPoints(xy, pAC);
			double dBC = getDistanceBetweenTwoPoints(xy, pBC);
			double lowest = dAB;
			XYColor closestPoint = pAB;
			if (dAC < lowest)
			{
				lowest = dAC;
				closestPoint = pAC;
			}
			if (dBC < lowest)
			{
				//lowest = dBC;
				closestPoint = pBC;
			}
			// Change the xy value to a value which is within the reach of the lamp.
			xy.x = closestPoint.x;
			xy.y = closestPoint.y;
		}
	}
	return xy;
}

double CiColor::crossProduct(XYColor p1, XYColor p2)
{
	return p1.x * p2.y - p1.y * p2.x;
}

bool CiColor::isPointInLampsReach(CiColor p, const CiColorTriangle& colorSpace)
{
	bool rc = false;
	XYColor v1 = { colorSpace.green.x - colorSpace.red.x, colorSpace.green.y - colorSpace.red.y };
	XYColor v2 = { colorSpace.blue.x - colorSpace.red.x, colorSpace.blue.y - colorSpace.red.y };
	XYColor  q = { p.x - colorSpace.red.x, p.y - colorSpace.red.y };
	double s = crossProduct(q, v2) / crossProduct(v1, v2);
	double t = crossProduct(v1, q) / crossProduct(v1, v2);
	if ((s >= 0.0) && (t >= 0.0) && (s + t <= 1.0))
	{
		rc = true;
	}
	return rc;
}

XYColor CiColor::getClosestPointToPoint(XYColor a, XYColor b, CiColor p)
{
	XYColor AP = { p.x - a.x, p.y - a.y };
	XYColor AB = { b.x - a.x, b.y - a.y };
	double ab2 = AB.x * AB.x + AB.y * AB.y;
	double ap_ab = AP.x * AB.x + AP.y * AB.y;
	double t = ap_ab / ab2;
	if (t < 0.0)
	{
		t = 0.0;
	}
	else if (t > 1.0)
	{
		t = 1.0;
	}
	return { a.x + AB.x * t, a.y + AB.y * t };
}

double CiColor::getDistanceBetweenTwoPoints(CiColor p1, XYColor p2)
{
	// Horizontal difference.
	double dx = p1.x - p2.x;
	// Vertical difference.
	double dy = p1.y - p2.y;
	// Absolute value.
	return sqrt(dx * dx + dy * dy);
}

LedDevicePhilipsHueBridge::LedDevicePhilipsHueBridge(const QJsonObject& deviceConfig)
	: ProviderUdpSSL(deviceConfig)
	, _restApi(nullptr)
	, _apiPort(API_DEFAULT_PORT)
	, _useHueEntertainmentAPI(false)
	, _api_major(0)
	, _api_minor(0)
	, _api_patch(0)
	, _isHueEntertainmentReady(false)
{
}

LedDevicePhilipsHueBridge::~LedDevicePhilipsHueBridge()
{
	delete _restApi;
	_restApi = nullptr;
}

bool LedDevicePhilipsHueBridge::init(const QJsonObject& deviceConfig)
{
	_useHueEntertainmentAPI = deviceConfig[CONFIG_USE_HUE_ENTERTAINMENT_API].toBool(false);	

	// Overwrite non supported/required features
	if (deviceConfig["refreshTime"].toInt(0) > 0)
	{
		InfoIf((!_useHueEntertainmentAPI), _log, "Device Philips Hue does not require setting refresh rate. Refresh time is ignored.");
		_devConfig["refreshTime"] = 0;
	}

	DebugIf(verbose, _log, "deviceConfig: [%s]", QString(QJsonDocument(_devConfig).toJson(QJsonDocument::Compact)).toUtf8().constData());

	bool isInitOK = false;

	if (LedDevice::init(deviceConfig))
	{

		log("DeviceType", "%s", QSTRING_CSTR(this->getActiveDeviceType()));
		log("LedCount", "%d", this->getLedCount());
		log("RefreshTime", "%d", _refreshTimerInterval_ms);

		//Set hostname as per configuration and_defaultHost default port
		QString address = deviceConfig[CONFIG_ADDRESS].toString();

		//If host not configured the init failed
		if (address.isEmpty())
		{
			this->setInError("No target hostname nor IP defined");
			isInitOK = false;
		}
		else
		{
			QStringList addressparts = QStringUtils::SPLITTER(address, ':');
			_hostname = addressparts[0];
			log("Hostname/IP", "%s", QSTRING_CSTR(_hostname));

			if (addressparts.size() > 1)
			{
				_apiPort = addressparts[1].toInt();
				log("Port", "%u", _apiPort);
			}

			_username = deviceConfig[CONFIG_USERNAME].toString();

			if (initRestAPI(_hostname, _apiPort, _username))
			{
				if (initMaps())
				{
					isInitOK = ProviderUdpSSL::init(_devConfig);
				}
				else
				{
					_currentRetry = _maxRetry;
					if (!_retryMode)
					{
						_retryMode = true;
						QTimer::singleShot(1000, [this]() { _retryMode = false; if (_currentRetry > 0) enableDevice(true);  });
					}
				}
			}
		}
	}

	return isInitOK;
}

bool LedDevicePhilipsHueBridge::initRestAPI(const QString& hostname, int port, const QString& token)
{
	if (_restApi == nullptr)
		_restApi = new ProviderRestApi(hostname, port);
	else
		_restApi->updateHost(hostname, port);

	_restApi->setBasePath(QString(API_BASE_PATH).arg(token));

	return true;
}

bool LedDevicePhilipsHueBridge::checkApiError(const QJsonDocument& response, bool supressError)
{
	bool apiError = false;
	QString errorReason;

	QString strJson(response.toJson(QJsonDocument::Compact));
	DebugIf(verbose, _log, "Reply: [%s]", strJson.toUtf8().constData());

	QVariantList rspList = response.toVariant().toList();
	if (!rspList.isEmpty())
	{
		QVariantMap map = rspList.first().toMap();
		if (map.contains(API_ERROR))
		{
			// API call failed to execute an error message was returned
			QString errorAddress = map.value(API_ERROR).toMap().value(API_ERROR_ADDRESS).toString();
			QString errorDesc = map.value(API_ERROR).toMap().value(API_ERROR_DESCRIPTION).toString();
			QString errorType = map.value(API_ERROR).toMap().value(API_ERROR_TYPE).toString();

			log("Error Type", "%s", QSTRING_CSTR(errorType));
			log("Error Address", "%s", QSTRING_CSTR(errorAddress));
			log("Error Address Description", "%s", QSTRING_CSTR(errorDesc));

			if (errorType != "901")
			{
				errorReason = QString("(%1) %2, Resource:%3").arg(errorType, errorDesc, errorAddress);

				if (!supressError)
				{
					this->setInError(errorReason);
				}
				else
					Warning(_log, "Suppresing error: %s", QSTRING_CSTR(errorReason));

				apiError = true;
			}
		}
	}
	return apiError;
}

const int* LedDevicePhilipsHueBridge::getCiphersuites() const
{
	return SSL_CIPHERSUITES;
}

void LedDevicePhilipsHueBridge::log(const char* msg, const char* type, ...) const
{
	const size_t max_val_length = 1024;
	char val[max_val_length];
	va_list args;
	va_start(args, type);
	vsnprintf(val, max_val_length, type, args);
	va_end(args);
	std::string s = msg;
	size_t max = 30;
	if (max > s.length())
		s.append(max - s.length(), ' ');
	Debug(_log, "%s: %s", s.c_str(), val);
}

QJsonDocument LedDevicePhilipsHueBridge::getAllBridgeInfos()
{
	return get(API_ROOT);
}

bool LedDevicePhilipsHueBridge::initMaps()
{
	bool isInitOK = true;

	QJsonDocument doc = getAllBridgeInfos();

	DebugIf(verbose, _log, "doc: [%s]", QString(QJsonDocument(doc).toJson(QJsonDocument::Compact)).toUtf8().constData());

	if (doc.isEmpty())
		setInError("Could not read the bridge details");

	if (this->isInError())
	{
		isInitOK = false;
	}
	else
	{
		setBridgeConfig(doc);
		if (_useHueEntertainmentAPI)
		{
			setGroupMap(doc);
		}
		setLightsMap(doc);
	}

	return isInitOK;
}

void LedDevicePhilipsHueBridge::setBridgeConfig(const QJsonDocument& doc)
{
	QJsonObject jsonConfigInfo = doc.object()[API_CONFIG].toObject();
	if (verbose)
	{
		std::cout << "jsonConfigInfo: [" << QString(QJsonDocument(jsonConfigInfo).toJson(QJsonDocument::Compact)).toUtf8().constData() << "]" << std::endl;
	}

	QString deviceName = jsonConfigInfo[DEV_DATA_NAME].toString();
	_deviceModel = jsonConfigInfo[DEV_DATA_MODEL].toString();
	QString deviceBridgeID = jsonConfigInfo[DEV_DATA_BRIDGEID].toString();
	_deviceFirmwareVersion = jsonConfigInfo[DEV_DATA_FIRMWAREVERSION].toString();
	_deviceAPIVersion = jsonConfigInfo[DEV_DATA_APIVERSION].toString();

	QStringList apiVersionParts = QStringUtils::SPLITTER(_deviceAPIVersion, '.');
	if (!apiVersionParts.isEmpty())
	{
		_api_major = apiVersionParts[0].toUInt();
		_api_minor = apiVersionParts[1].toUInt();
		_api_patch = apiVersionParts[2].toUInt();

		if (_api_major > 1 || (_api_major == 1 && _api_minor >= 22))
		{
			_isHueEntertainmentReady = true;
		}
	}

	if (_useHueEntertainmentAPI)
	{
		DebugIf(!_isHueEntertainmentReady, _log, "Bridge is not Entertainment API Ready - Entertainment API usage was disabled!");
		_useHueEntertainmentAPI = _isHueEntertainmentReady;
	}

	log("Bridge Name", "%s", QSTRING_CSTR(deviceName));
	log("Model", "%s", QSTRING_CSTR(_deviceModel));
	log("Bridge-ID", "%s", QSTRING_CSTR(deviceBridgeID));
	log("SoftwareVersion", "%s", QSTRING_CSTR(_deviceFirmwareVersion));
	log("API-Version", "%u.%u.%u", _api_major, _api_minor, _api_patch);
	log("EntertainmentReady", "%d", static_cast<int>(_isHueEntertainmentReady));
}

void LedDevicePhilipsHueBridge::setLightsMap(const QJsonDocument& doc)
{
	QJsonObject jsonLightsInfo = doc.object()[API_LIGHTS].toObject();

	DebugIf(verbose, _log, "jsonLightsInfo: [%s]", QString(QJsonDocument(jsonLightsInfo).toJson(QJsonDocument::Compact)).toUtf8().constData());

	// Get all available light ids and their values
	QStringList keys = jsonLightsInfo.keys();

	_ledCount = static_cast<uint>(keys.size());
	_lightsMap.clear();

	for (int i = 0; i < static_cast<int>(_ledCount); ++i)
	{
		_lightsMap.insert(keys.at(i).toUShort(), jsonLightsInfo.take(keys.at(i)).toObject());
	}

	if (getLedCount() == 0)
	{
		this->setInError("No light-IDs found at the Philips Hue Bridge");
	}
	else
	{
		log("Lights in Bridge found", "%d", getLedCount());
	}
}

void LedDevicePhilipsHueBridge::setGroupMap(const QJsonDocument& doc)
{
	QJsonObject jsonGroupsInfo = doc.object()[API_GROUPS].toObject();

	DebugIf(verbose, _log, "jsonGroupsInfo: [%s]", QString(QJsonDocument(jsonGroupsInfo).toJson(QJsonDocument::Compact)).toUtf8().constData());

	// Get all available group ids and their values
	QStringList keys = jsonGroupsInfo.keys();

	int _groupsCount = keys.size();
	_groupsMap.clear();

	for (int i = 0; i < _groupsCount; ++i)
	{
		_groupsMap.insert(keys.at(i).toUShort(), jsonGroupsInfo.take(keys.at(i)).toObject());
	}
}

QMap<quint16, QJsonObject> LedDevicePhilipsHueBridge::getLightMap() const
{
	return _lightsMap;
}

QMap<quint16, QJsonObject> LedDevicePhilipsHueBridge::getGroupMap() const
{
	return _groupsMap;
}

QString LedDevicePhilipsHueBridge::getGroupName(quint16 groupId) const
{
	QString groupName;
	if (_groupsMap.contains(groupId))
	{
		QJsonObject group = _groupsMap.value(groupId);
		groupName = group.value(API_GROUP_NAME).toString().trimmed().replace("\"", "");
	}
	else
	{
		Error(_log, "Group ID %u doesn't exists on this bridge", groupId);
	}
	return groupName;
}

QJsonArray LedDevicePhilipsHueBridge::getGroupLights(quint16 groupId) const
{
	QJsonArray groupLights;
	// search user groupid inside _groupsMap and create light if found
	if (_groupsMap.contains(groupId))
	{
		QJsonObject group = _groupsMap.value(groupId);

		if (group.value(API_GROUP_TYPE) == API_GROUP_TYPE_ENTERTAINMENT)
		{
			QString groupName = getGroupName(groupId);
			groupLights = group.value(API_LIGHTS).toArray();

			log("Entertainment Group found", "[%d] %s", groupId, QSTRING_CSTR(groupName));
			log("Lights in Group", "%d", groupLights.size());
			Info(_log, "Entertainment Group [%d] \"%s\" with %d Lights found", groupId, QSTRING_CSTR(groupName), groupLights.size());
		}
		else
		{
			Error(_log, "Group ID %d is not an entertainment group", groupId);
		}
	}
	else
	{
		Error(_log, "Group ID %d doesn't exists on this bridge", groupId);
	}

	return groupLights;
}


QJsonDocument LedDevicePhilipsHueBridge::getLightState(unsigned int lightId)
{
	DebugIf(verbose, _log, "GetLightState [%u]", lightId);
	return get(QString("%1/%2").arg(API_LIGHTS).arg(lightId));
}

void LedDevicePhilipsHueBridge::setLightState(unsigned int lightId, const QString& state)
{
	DebugIf(verbose, _log, "SetLightState [%u]: %s", lightId, QSTRING_CSTR(state));
	post(QString("%1/%2/%3").arg(API_LIGHTS).arg(lightId).arg(API_STATE), state);
}

QJsonDocument LedDevicePhilipsHueBridge::getGroupState(unsigned int groupId)
{
	DebugIf(verbose, _log, "GetGroupState [%u]", groupId);
	return get(QString("%1/%2").arg(API_GROUPS).arg(groupId));
}

QJsonDocument LedDevicePhilipsHueBridge::setGroupState(unsigned int groupId, bool state)
{
	QString active = state ? API_STREAM_ACTIVE_VALUE_TRUE : API_STREAM_ACTIVE_VALUE_FALSE;
	return post(QString("%1/%2").arg(API_GROUPS).arg(groupId), QString("{\"%1\":{\"%2\":%3}}").arg(API_STREAM, API_STREAM_ACTIVE, active), true);
}

QJsonDocument LedDevicePhilipsHueBridge::get(const QString& route)
{	
	_restApi->setPath(route);

	httpResponse response = _restApi->get();		

	checkApiError(response.getBody());
	return response.getBody();
}

QJsonDocument LedDevicePhilipsHueBridge::post(const QString& route, const QString& content, bool supressError)
{
	_restApi->setPath(route);

	httpResponse response = _restApi->put(content);
	checkApiError(response.getBody(), supressError);
	return response.getBody();
}

bool LedDevicePhilipsHueBridge::isStreamOwner(const QString& streamOwner) const
{
	return (streamOwner != "" && streamOwner == _username);
}

const std::set<QString> PhilipsHueLight::GAMUT_A_MODEL_IDS =
{ "LLC001", "LLC005", "LLC006", "LLC007", "LLC010", "LLC011", "LLC012", "LLC013", "LLC014", "LST001" };
const std::set<QString> PhilipsHueLight::GAMUT_B_MODEL_IDS =
{ "LCT001", "LCT002", "LCT003", "LCT007", "LLM001" };
const std::set<QString> PhilipsHueLight::GAMUT_C_MODEL_IDS =
{ "LCA001", "LCA002", "LCA003", "LCG002", "LCP001", "LCP002", "LCT010", "LCT011", "LCT012", "LCT014", "LCT015", "LCT016", "LCT024", "LLC020", "LST002" };

PhilipsHueLight::PhilipsHueLight(Logger* log, unsigned int id, QJsonObject values, unsigned int ledidx, int onBlackTimeToPowerOff,
	int onBlackTimeToPowerOn)
	: _log(log)
	, _id(id)
	, _ledidx(ledidx)
	, _on(false)
	, _transitionTime(0)
	, _color({ 0.0, 0.0, 0.0 })
	, _hasColor(false)
	, _colorBlack({ 0.0, 0.0, 0.0 })
	, _modelId(values[API_MODEID].toString().trimmed().replace("\"", ""))
	, _lastSendColor(0)
	, _lastBlack(-1)
	, _lastWhite(-1)
	, _blackScreenTriggered(false)
	, _onBlackTimeToPowerOff(onBlackTimeToPowerOff)
	, _onBlackTimeToPowerOn(onBlackTimeToPowerOn)
{
	// Find id in the sets and set the appropriate color space.
	if (GAMUT_A_MODEL_IDS.find(_modelId) != GAMUT_A_MODEL_IDS.end())
	{
		Debug(_log, "Recognized model id %s of light ID %d as gamut A", QSTRING_CSTR(_modelId), id);
		_colorSpace.red = { 0.704, 0.296 };
		_colorSpace.green = { 0.2151, 0.7106 };
		_colorSpace.blue = { 0.138, 0.08 };
		_colorBlack = { 0.138, 0.08, 0.0 };
	}
	else if (GAMUT_B_MODEL_IDS.find(_modelId) != GAMUT_B_MODEL_IDS.end())
	{
		Debug(_log, "Recognized model id %s of light ID %d as gamut B", QSTRING_CSTR(_modelId), id);
		_colorSpace.red = { 0.675, 0.322 };
		_colorSpace.green = { 0.409, 0.518 };
		_colorSpace.blue = { 0.167, 0.04 };
		_colorBlack = { 0.167, 0.04, 0.0 };
	}
	else if (GAMUT_C_MODEL_IDS.find(_modelId) != GAMUT_C_MODEL_IDS.end())
	{
		Debug(_log, "Recognized model id %s of light ID %d as gamut C", QSTRING_CSTR(_modelId), id);
		_colorSpace.red = { 0.6915, 0.3083 };
		_colorSpace.green = { 0.17, 0.7 };
		_colorSpace.blue = { 0.1532, 0.0475 };
		_colorBlack = { 0.1532, 0.0475, 0.0 };
	}
	else
	{
		Warning(_log, "Did not recognize model id %s of light ID %d", QSTRING_CSTR(_modelId), id);
		_colorSpace.red = { 1.0, 0.0 };
		_colorSpace.green = { 0.0, 1.0 };
		_colorSpace.blue = { 0.0, 0.0 };
		_colorBlack = { 0.0, 0.0, 0.0 };
	}

	_lightname = values["name"].toString().trimmed().replace("\"", "");
	Info(_log, "Light ID %d (\"%s\", LED index \"%d\", onBlackTimeToPowerOff: %d, _onBlackTimeToPowerOn: %d) created", id, QSTRING_CSTR(_lightname), ledidx, _onBlackTimeToPowerOff, _onBlackTimeToPowerOn);
}

void PhilipsHueLight::blackScreenTriggered()
{
	_blackScreenTriggered = true;
}

bool PhilipsHueLight::isBusy()
{
	bool temp = true;

	uint64_t _currentTime = QDateTime::currentMSecsSinceEpoch();
	if ((int64_t)(_currentTime - _lastSendColor) >= (int64_t)100)
	{
		_lastSendColor = _currentTime;
		temp = false;
	}

	return temp;
}

void PhilipsHueLight::setBlack()
{
	CiColor black;
	black.bri = 0;
	black.x = 0;
	black.y = 0;
	setColor(black);
}

bool PhilipsHueLight::isBlack(bool isBlack)
{
	if (!isBlack)
	{
		_lastBlack = 0;
		return false;
	}

	if (_lastBlack == 0)
	{
		_lastBlack = QDateTime::currentMSecsSinceEpoch();
		return false;
	}

	uint64_t _currentTime = QDateTime::currentMSecsSinceEpoch();
	if ((int64_t)(_currentTime - _lastBlack) >= (int64_t)_onBlackTimeToPowerOff)
	{
		return true;
	}

	return false;
}

bool PhilipsHueLight::isWhite(bool isWhite)
{
	if (!isWhite)
	{
		_lastWhite = 0;
		return false;
	}

	if (_lastWhite == 0)
	{
		_lastWhite = QDateTime::currentMSecsSinceEpoch();
		return false;
	}

	uint64_t _currentTime = QDateTime::currentMSecsSinceEpoch();
	if ((int64_t)(_currentTime - _lastWhite) >= (int64_t)_onBlackTimeToPowerOn)
	{
		return true;
	}

	return false;
}

PhilipsHueLight::~PhilipsHueLight()
{
	DebugIf(verbose, _log, "Light ID %d (\"%s\", LED index \"%d\") deconstructed", _id, QSTRING_CSTR(_lightname), _ledidx);
}

unsigned int PhilipsHueLight::getId() const
{
	return _id;
}

QString PhilipsHueLight::getOriginalState() const
{
	return _originalState;
}

void PhilipsHueLight::saveOriginalState(const QJsonObject& values)
{
	if (_blackScreenTriggered)
	{
		_blackScreenTriggered = false;
		return;
	}

	// Get state object values which are subject to change.
	if (!values[API_STATE].toObject().contains("on"))
	{
		Error(_log, "Got invalid state object from light ID %d", _id);
	}
	QJsonObject lState = values[API_STATE].toObject();
	_originalStateJSON = lState;

	QJsonObject state;
	state["on"] = lState["on"];
	_originalColor = CiColor();
	_originalColor.bri = 0;
	_originalColor.x = 0;
	_originalColor.y = 0;
	QString c;
	if (state[API_STATE_ON].toBool())
	{
		state[API_XY_COORDINATES] = lState[API_XY_COORDINATES];
		state[API_BRIGHTNESS] = lState[API_BRIGHTNESS];
		_on = true;
		_color = {
			state[API_XY_COORDINATES].toArray()[0].toDouble(),
			state[API_XY_COORDINATES].toArray()[1].toDouble(),
			state[API_BRIGHTNESS].toDouble() / 254.0
		};
		_originalColor = _color;
		c = QString("{ \"%1\": [%2, %3], \"%4\": %5 }").arg(API_XY_COORDINATES).arg(_originalColor.x, 0, 'd', 4).arg(_originalColor.y, 0, 'd', 4).arg(API_BRIGHTNESS).arg((_originalColor.bri * 254.0), 0, 'd', 4);
		Debug(_log, "Philips original state stored: %s", QSTRING_CSTR(c));
		_transitionTime = values[API_STATE].toObject()[API_TRANSITIONTIME].toInt();
	}
	//Determine the original state.
	_originalState = QJsonDocument(state).toJson(QJsonDocument::JsonFormat::Compact).trimmed();
}

void PhilipsHueLight::setOnOffState(bool on)
{
	this->_on = on;
}

void PhilipsHueLight::setTransitionTime(int transitionTime)
{
	this->_transitionTime = transitionTime;
}

void PhilipsHueLight::setColor(const CiColor& color)
{
	this->_hasColor = true;
	this->_color = color;
}

bool PhilipsHueLight::getOnOffState() const
{
	return _on;
}

int PhilipsHueLight::getTransitionTime() const
{
	return _transitionTime;
}

CiColor PhilipsHueLight::getColor() const
{
	return _color;
}

bool PhilipsHueLight::hasColor()
{
	return _hasColor;
}

CiColorTriangle PhilipsHueLight::getColorSpace() const
{
	return _colorSpace;
}

LedDevicePhilipsHue::LedDevicePhilipsHue(const QJsonObject& deviceConfig)
	: LedDevicePhilipsHueBridge(deviceConfig)
	, _switchOffOnBlack(false)
	, _brightnessFactor(1.0)
	, _transitionTime(1)
	, _isInitLeds(false)
	, _lightsCount(0)
	, _groupId(0)
	, _blackLightsTimeout(15000)
	, _blackLevel(0.0)
	, _onBlackTimeToPowerOff(100)
	, _onBlackTimeToPowerOn(100)
	, _candyGamma(true)
	, _handshake_timeout_min(600)
	, _handshake_timeout_max(2000)
	, _stopConnection(false)
	, _lastConfirm(0)
	, _lastId(-1)
	, _groupStreamState(false)
{	
}

LedDevice* LedDevicePhilipsHue::construct(const QJsonObject& deviceConfig)
{
	return new LedDevicePhilipsHue(deviceConfig);
}

LedDevicePhilipsHue::~LedDevicePhilipsHue()
{
}

bool LedDevicePhilipsHue::setLights()
{
	bool isInitOK = true;

	_lightIds.clear();

	QJsonArray lArray;

	if (_useHueEntertainmentAPI && _groupId > 0)
	{
		lArray = getGroupLights(_groupId);
	}

	if (lArray.empty())
	{
		if (_useHueEntertainmentAPI)
		{
			_useHueEntertainmentAPI = false;
			Error(_log, "Group-ID [%u] is not usable - Entertainment API usage was disabled!", _groupId);
		}
		lArray = _devConfig[CONFIG_LIGHTIDS].toArray();
	}

	QString lightIDStr;

	if (!lArray.empty())
	{
		for (const QJsonValueRef id : lArray)
		{
			unsigned int lightId = id.toString().toUInt();
			if (lightId > 0)
			{
				if (std::find(_lightIds.begin(), _lightIds.end(), lightId) == _lightIds.end())
				{
					_lightIds.emplace_back(lightId);
					if (!lightIDStr.isEmpty())
					{
						lightIDStr.append(", ");
					}
					lightIDStr.append(QString::number(lightId));
				}
			}
		}
		std::sort(_lightIds.begin(), _lightIds.end());
	}

	unsigned int configuredLightsCount = static_cast<unsigned int>(_lightIds.size());

	log("Light-IDs configured", "%d", configuredLightsCount);

	if (configuredLightsCount == 0)
	{
		this->setInError("No light-IDs configured");
		isInitOK = false;
	}
	else
	{
		log("Light-IDs", "%s", QSTRING_CSTR(lightIDStr));
		isInitOK = updateLights(getLightMap());
	}

	return isInitOK;
}


bool LedDevicePhilipsHue::initLeds()
{
	bool isInitOK = false;

	if (!this->isInError())
	{
		if (setLights())
		{
			if (_useHueEntertainmentAPI)
			{
				_groupName = getGroupName(_groupId);
				_devConfig["host"] = _hostname;
				_devConfig["sslport"] = API_SSL_SERVER_PORT;
				_devConfig["servername"] = API_SSL_SERVER_NAME;
				_devConfig["refreshTime"] = static_cast<int>(STREAM_REFRESH_TIME.count());
				_devConfig["psk"] = _devConfig[CONFIG_CLIENTKEY].toString();
				_devConfig["psk_identity"] = _devConfig[CONFIG_USERNAME].toString();
				_devConfig["seed_custom"] = API_SSL_SEED_CUSTOM;
				_devConfig["retry_left"] = _maxRetry;
				_devConfig["hs_attempts"] = STREAM_SSL_HANDSHAKE_ATTEMPTS;
				_devConfig["hs_timeout_min"] = static_cast<int>(_handshake_timeout_min);
				_devConfig["hs_timeout_max"] = static_cast<int>(_handshake_timeout_max);

				isInitOK = ProviderUdpSSL::init(_devConfig);

			}
			else
			{
				// adapt refreshTime to count of user lightIds (bridge 10Hz max overall)
				setRefreshTime(static_cast<int>(100 * getLightsCount()));
				isInitOK = true;
			}
			_isInitLeds = true;
		}
		else
		{
			isInitOK = false;
		}
	}

	return isInitOK;
}

bool LedDevicePhilipsHue::init(const QJsonObject& deviceConfig)
{
	_configBackup = deviceConfig;

	verbose = deviceConfig[CONFIG_VERBOSE].toBool(false);

	// Initialise LedDevice configuration and execution environment
	_switchOffOnBlack = _devConfig[CONFIG_ON_OFF_BLACK].toBool(true);
	_blackLightsTimeout = _devConfig[CONFIG_BLACK_LIGHTS_TIMEOUT].toInt(15000);
	_brightnessFactor = _devConfig[CONFIG_BRIGHTNESSFACTOR].toDouble(1.0);
	_transitionTime = _devConfig[CONFIG_TRANSITIONTIME].toInt(1);
	_isRestoreOrigState = _devConfig[CONFIG_RESTORE_STATE].toBool(true);
	_groupId = static_cast<quint16>(_devConfig[CONFIG_GROUPID].toInt(0));
	_blackLevel = _devConfig["blackLevel"].toDouble(0.0);
	_onBlackTimeToPowerOff = _devConfig["onBlackTimeToPowerOff"].toInt(100);
	_onBlackTimeToPowerOn = _devConfig["onBlackTimeToPowerOn"].toInt(100);
	_candyGamma = _devConfig["candyGamma"].toBool(true);
	_handshake_timeout_min = _devConfig["handshakeTimeoutMin"].toInt(300);
	_handshake_timeout_max = _devConfig["handshakeTimeoutMax"].toInt(1000);
	_maxRetry = _devConfig["maxRetry"].toInt(60);

	if (_blackLevel < 0.0) { _blackLevel = 0.0; }
	if (_blackLevel > 1.0) { _blackLevel = 1.0; }

	log("Max. retry count", "%d", _maxRetry);
	log("Off on Black", "%d", static_cast<int>(_switchOffOnBlack));
	log("Brightness Factor", "%f", _brightnessFactor);
	log("Transition Time", "%d", _transitionTime);
	log("Restore Original State", "%d", static_cast<int>(_isRestoreOrigState));
	log("Use Hue Entertainment API", "%d", static_cast<int>(_useHueEntertainmentAPI));
	log("Brightness Threshold", "%f", _blackLevel);
	log("CandyGamma", "%d", static_cast<int>(_candyGamma));
	log("Time to power off the lamp on black", "%d", _onBlackTimeToPowerOff);
	log("Time to power on the lamp on signal", "%d", _onBlackTimeToPowerOn);
	log("SSL Handshake min", "%d", _handshake_timeout_min);
	log("SSL Handshake max", "%d", _handshake_timeout_max);

	if (_useHueEntertainmentAPI)
	{
		log("Entertainment API Group-ID", "%d", _groupId);

		if (_groupId == 0)
		{
			Error(_log, "Disabling usage of HueEntertainmentAPI: Group-ID is invalid", "%d", _groupId);
			_useHueEntertainmentAPI = false;
		}
	}

	bool isInitOK = LedDevicePhilipsHueBridge::init(deviceConfig);

	if (isInitOK)
		isInitOK = initLeds();


	return isInitOK;
}

bool LedDevicePhilipsHue::openStream()
{
	bool isInitOK = false;
	bool streamState = getStreamGroupState();

	if (!this->isInError())
	{
		// stream is already active
		if (streamState)
		{
			// if same owner stop stream
			if (isStreamOwner(_streamOwner))
			{
				Debug(_log, "Group: \"%s\" [%u] is in use, try to stop stream", QSTRING_CSTR(_groupName), _groupId);

				if (stopStream())
				{
					Debug(_log, "Stream successful stopped");
					//Restore Philips Hue devices state
					restoreState();
					isInitOK = startStream();
				}
				else
				{
					Error(_log, "Group: \"%s\" [%u] couldn't stop by user: \"%s\" - Entertainment API not usable", QSTRING_CSTR(_groupName), _groupId, QSTRING_CSTR(_streamOwner));
				}
			}
			else
			{
				Error(_log, "Group: \"%s\" [%u] is in use and owned by other user: \"%s\" - Entertainment API not usable", QSTRING_CSTR(_groupName), _groupId, QSTRING_CSTR(_streamOwner));
			}
		}
		else
		{
			isInitOK = startStream();
		}
		if (isInitOK)
			storeState();
	}

	if (isInitOK)
	{
		// open UDP SSL Connection
		isInitOK = ProviderUdpSSL::initNetwork();

		if (isInitOK)
		{
			Info(_log, "Philips Hue Entertaiment API successful connected! Start Streaming.");
		}
		else
		{
			Error(_log, "Philips Hue Entertaiment API not connected!");
		}
	}
	else
	{
		Error(_log, "Philips Hue Entertaiment API could not initialisized!");
	}

	return isInitOK;
}

bool LedDevicePhilipsHue::updateLights(const QMap<quint16, QJsonObject>& map)
{
	bool isInitOK = true;

	// search user lightid inside map and create light if found
	_lights.clear();

	if (!_lightIds.empty())
	{
		unsigned int ledidx = 0;
		_lights.reserve(_lightIds.size());
		for (const auto id : _lightIds)
		{
			if (map.contains(id))
			{
				_lights.emplace_back(_log, id, map.value(id), ledidx, _onBlackTimeToPowerOff, _onBlackTimeToPowerOn);
			}
			else
			{
				Warning(_log, "Configured light-ID %d is not available at this bridge", id);
			}
			ledidx++;
		}
	}

	unsigned int lightsCount = static_cast<unsigned int>(_lights.size());

	setLightsCount(lightsCount);

	if (lightsCount == 0)
	{
		Error(_log, "No usable lights found!");
		isInitOK = false;
	}

	return isInitOK;
}




bool LedDevicePhilipsHue::startStream()
{
	bool rc = true;

	Debug(_log, "Start entertainment stream");

	rc = setStreamGroupState(true);

	if (rc)
		Debug(_log, "The stream has start");
	else
		this->setInError("The stream has NOT start. Give up.");

	return rc;
}

bool LedDevicePhilipsHue::stopStream()
{
	bool canRestore = false;

	// Set to black
	if (!_lightIds.empty() && _isDeviceReady && !_stopConnection && _useHueEntertainmentAPI)
	{
		if (!_isRestoreOrigState)
		{
			for (PhilipsHueLight& light : _lights)
			{
				light.setBlack();
			}

			for (int i = 0; i < 2; i++)
			{
				writeStream(true);
				QThread::msleep(5);
			}

			QThread::msleep(25);
		}
		else
			canRestore = true;
	}

	ProviderUdpSSL::closeNetwork();

	int index = 3;
	while (!setStreamGroupState(false) && --index > 0)
	{
		Debug(_log, "Stop entertainment stream. Retrying...");
		QThread::msleep(500);
	}

	if (canRestore && _isRestoreOrigState)
		restoreState();

	bool rc = (index > 0);

	if (rc)
		Debug(_log, "The stream has stopped");
	else
		this->setInError("The stream has NOT stopped. Give up.");

	return rc;
}

bool LedDevicePhilipsHue::setStreamGroupState(bool state)
{
	QString active = state ? API_STREAM_ACTIVE_VALUE_TRUE : API_STREAM_ACTIVE_VALUE_FALSE;

	QJsonDocument doc = setGroupState(_groupId, state);

	QVariant rsp = doc.toVariant();

	QVariantList list = rsp.toList();
	if (!list.isEmpty())
	{
		QVariantMap map = list.first().toMap();

		if (!map.contains(API_SUCCESS))
		{
			Warning(_log, "%s", QSTRING_CSTR(QString("set stream to %1: Neither error nor success contained in Bridge response...").arg(active)));
		}
		else
		{
			//Check original Hue response {"success":{"/groups/groupID/stream/active":activeYesNo}}
			QString valueName = QString(API_STREAM_RESPONSE_FORMAT).arg(API_GROUPS).arg(_groupId).arg(API_STREAM, API_STREAM_ACTIVE);
			if (!map.value(API_SUCCESS).toMap().value(valueName).isValid())
			{
				//Workaround
				//Check diyHue response   {"success":{"/groups/groupID/stream":{"active":activeYesNo}}}
				QString diyHueValueName = QString("/%1/%2/%3").arg(API_GROUPS).arg(_groupId).arg(API_STREAM);
				QJsonObject diyHueActiveState = map.value(API_SUCCESS).toMap().value(diyHueValueName).toJsonObject();

				if (diyHueActiveState.isEmpty())
				{
					this->setInError(QString("set stream to %1: Bridge response is not Valid").arg(active));
				}
				else
				{
					_groupStreamState = diyHueActiveState[API_STREAM_ACTIVE].toBool();
					return (_groupStreamState == state);
				}
			}
			else
			{
				_groupStreamState = map.value(API_SUCCESS).toMap().value(valueName).toBool();
				return (_groupStreamState == state);
			}
		}
	}

	return false;
}

bool LedDevicePhilipsHue::getStreamGroupState()
{
	QJsonDocument doc = getGroupState(_groupId);

	if (!this->isInError())
	{
		QJsonObject obj = doc.object()[API_STREAM].toObject();

		if (obj.isEmpty())
		{
			this->setInError("no Streaming Infos in Group found");
			return false;
		}
		else
		{
			_streamOwner = obj.value(API_STREAM_OWNER).toString();
			bool streamState = obj.value(API_STREAM_ACTIVE).toBool();
			return streamState;
		}
	}

	return false;
}

QByteArray LedDevicePhilipsHue::prepareStreamData() const
{
	QByteArray msg;
	msg.reserve(static_cast<int>(sizeof(HEADER) + sizeof(PAYLOAD_PER_LIGHT) * _lights.size()));
	msg.append(reinterpret_cast<const char*>(HEADER), sizeof(HEADER));

	for (const PhilipsHueLight& light : _lights)
	{
		CiColor lightC = light.getColor();
		quint64 R = lightC.x * 0xffff;
		quint64 G = lightC.y * 0xffff;
		quint64 B = lightC.bri * 0xffff;
		unsigned int id = light.getId();
		const uint8_t payload[] = {
			0x00, 0x00, static_cast<uint8_t>(id),
			static_cast<uint8_t>((R >> 8) & 0xff), static_cast<uint8_t>(R & 0xff),
			static_cast<uint8_t>((G >> 8) & 0xff), static_cast<uint8_t>(G & 0xff),
			static_cast<uint8_t>((B >> 8) & 0xff), static_cast<uint8_t>(B & 0xff)
		};
		msg.append(reinterpret_cast<const char*>(payload), sizeof(payload));
	}

	return msg;
}

int LedDevicePhilipsHue::close()
{
	int retval = 0;

	_isDeviceReady = false;

	return retval;
}

void LedDevicePhilipsHue::stop()
{
	_currentRetry = 0;

	LedDevicePhilipsHueBridge::stop();
}

int LedDevicePhilipsHue::open()
{
	int retval = 0;

	_isDeviceReady = true;

	return retval;
}


bool LedDevicePhilipsHue::switchOn()
{
	bool rc = false;

	if (_retryMode)
		return false;

	Info(_log, "Switching ON Philips Hue device");	

	try
	{
		if (_isOn)
		{
			_currentRetry = 0;

			Debug(_log, "Philips is already enabled. Skipping.");
			rc = true;
		}
		else
		{
			if (!_isDeviceInitialised && initMaps())
			{
				init(_configBackup);
				_currentRetry = 0;
				_isEnabled = true;
				_isDeviceInitialised = true;
			}
				
			if (_isDeviceInitialised)
			{
				_isEnabled = true;				

				if (_useHueEntertainmentAPI)
				{
					if (openStream())
					{
						_isOn = true;
						rc = true;
					}
				}
				else
				{
					storeState();
					if (powerOn())
					{
						_isOn = true;
						rc = true;
					}
				}
			}

			if (!_isOn && _maxRetry > 0)
			{
				if (_currentRetry <= 0)
					_currentRetry = _maxRetry + 1;

				_currentRetry--;

				if (_currentRetry > 0)
					Warning(_log, "The PhilipsHue device is not ready... will try to reconnect (try %i/%i).", (_maxRetry - _currentRetry + 1), _maxRetry);
				else
					Error(_log, "The PhilipsHue device is not ready... give up.");


				if (_currentRetry > 0)
				{
					_retryMode = true;
					QTimer::singleShot(1000, [this]() { _retryMode = false; if (_currentRetry > 0) enableDevice(true);  });
				}
			}
			else
				_currentRetry = 0;
		}
	}
	catch (...)
	{
		rc = false;
		Error(_log, "Philips Hue error detected (ON)");
	}

	if (rc && _isOn)
		Info(_log, "Philips Hue device is ON");
	else
		Warning(_log, "Could not set Philips Hue device ON");

	return rc;
}

bool LedDevicePhilipsHue::switchOff()
{
	bool result = false;

	Info(_log, "Switching OFF Philips Hue device");

	_currentRetry = 0;

	try
	{
		if (_useHueEntertainmentAPI && _groupStreamState)
		{
			result = stopStream();
			_isOn = false;
		}
		else
			result = LedDevicePhilipsHueBridge::switchOff();
	}
	catch (...)
	{
		result = false;
		Error(_log, "Philips Hue error detected (OFF)");
	}

	if (result && !_isOn)
		Info(_log, "Philips Hue device is OFF");
	else
		Warning(_log, "Could not set Philips Hue device OFF");

	return result;
}

int LedDevicePhilipsHue::write(const std::vector<ColorRgb>& ledValues)
{
	// lights will be empty sometimes
	if (_lights.empty())
	{
		return -1;
	}

	// more lights then leds, stop always
	if (ledValues.size() < getLightsCount())
	{
		Error(_log, "More light-IDs configured than leds, each light-ID requires one led!");
		return -1;
	}

	writeSingleLights(ledValues);

	if (_useHueEntertainmentAPI && _isInitLeds)
	{
		writeStream();
	}

	return 0;
}

int LedDevicePhilipsHue::writeSingleLights(const std::vector<ColorRgb>& ledValues)
{
	// Iterate through lights and set colors.
	unsigned int idx = 0;
	for (PhilipsHueLight& light : _lights)
	{
		// Get color.
		ColorRgb color = ledValues.at(idx);
		// Scale colors from [0, 255] to [0, 1] and convert to xy space.
		CiColor xy = CiColor::rgbToCiColor(color.red / 255.0, color.green / 255.0, color.blue / 255.0, light.getColorSpace(), _candyGamma);


		if (_switchOffOnBlack && xy.bri <= _blackLevel && light.isBlack(true))
		{
			xy.bri = 0;
			xy.x = 0;
			xy.y = 0;

			if (_useHueEntertainmentAPI)
			{
				if (light.getOnOffState())
				{
					this->setColor(light, xy);
					this->setOnOffState(light, false);
				}
			}
			else
			{
				if (light.getOnOffState())
					setState(light, false, xy);
			}
		}
		else
		{
			bool currentstate = light.getOnOffState();

			if (_switchOffOnBlack && xy.bri > _blackLevel && light.isWhite(true))
			{
				if (!currentstate)
					xy.bri = xy.bri / 2;

				if (_useHueEntertainmentAPI)
				{
					this->setOnOffState(light, true);
					this->setColor(light, xy);
				}
				else
					this->setState(light, true, xy);
			}
			else if (!_switchOffOnBlack)
			{
				if (_useHueEntertainmentAPI)
				{
					this->setOnOffState(light, true);
					this->setColor(light, xy);
				}
				else
					this->setState(light, true, xy);
			}
		}

		if (xy.bri > _blackLevel)
			light.isBlack(false);
		if (xy.bri <= _blackLevel)
			light.isWhite(false);

		idx++;
	}

	return 0;
}

void LedDevicePhilipsHue::writeStream(bool flush)
{
	QByteArray streamData = prepareStreamData();
	writeBytes(static_cast<uint>(streamData.size()), reinterpret_cast<unsigned char*>(streamData.data()), flush);
}

void LedDevicePhilipsHue::setOnOffState(PhilipsHueLight& light, bool on, bool force)
{
	if (light.getOnOffState() != on || force)
	{
		light.setOnOffState(on);
		QString state = on ? API_STATE_VALUE_TRUE : API_STATE_VALUE_FALSE;
		setLightState(light.getId(), QString("{\"%1\": %2 }").arg(API_STATE_ON, state));
	}
}

void LedDevicePhilipsHue::setTransitionTime(PhilipsHueLight& light)
{
	if (light.getTransitionTime() != _transitionTime)
	{
		light.setTransitionTime(_transitionTime);
		setLightState(light.getId(), QString("{\"%1\": %2 }").arg(API_TRANSITIONTIME).arg(_transitionTime));
	}
}

void LedDevicePhilipsHue::setColor(PhilipsHueLight& light, CiColor& color)
{
	if (!light.hasColor() || light.getColor() != color)
	{
		if (!_useHueEntertainmentAPI)
		{
			const int bri = qRound(qMin(254.0, _brightnessFactor * qMax(1.0, color.bri * 254.0)));
			QString stateCmd = QString("{\"%1\":[%2,%3],\"%4\":%5}").arg(API_XY_COORDINATES).arg(color.x, 0, 'd', 4).arg(color.y, 0, 'd', 4).arg(API_BRIGHTNESS).arg(bri);
			setLightState(light.getId(), stateCmd);
		}
		else
		{
			color.bri = (qMin((double)1.0, _brightnessFactor * qMax((double)0, color.bri)));
			//if(color.x == 0.0 && color.y == 0.0) color = colorBlack;
		}
		light.setColor(color);
	}
}

void LedDevicePhilipsHue::setState(PhilipsHueLight& light, bool on, const CiColor& color)
{
	QString stateCmd, powerCmd;;
	bool priority = false;

	if (light.getOnOffState() != on)
	{
		light.setOnOffState(on);
		QString state = on ? API_STATE_VALUE_TRUE : API_STATE_VALUE_FALSE;
		powerCmd = QString("\"%1\":%2").arg(API_STATE_ON, state);
		priority = true;
	}

	if (light.getTransitionTime() != _transitionTime)
	{
		light.setTransitionTime(_transitionTime);
		stateCmd += QString("\"%1\":%2,").arg(API_TRANSITIONTIME).arg(_transitionTime);
	}

	const int bri = qRound(qMin(254.0, _brightnessFactor * qMax(1.0, color.bri * 254.0)));
	if (!light.hasColor() || light.getColor() != color)
	{
		if (!light.isBusy() || priority)
		{
			light.setColor(color);
			stateCmd += QString("\"%1\":[%2,%3],\"%4\":%5,").arg(API_XY_COORDINATES).arg(color.x, 0, 'd', 4).arg(color.y, 0, 'd', 4).arg(API_BRIGHTNESS).arg(bri);
		}
	}

	if (!stateCmd.isEmpty() || !powerCmd.isEmpty())
	{
		if (!stateCmd.isEmpty())
		{
			stateCmd = QString("\"%1\":%2").arg(API_STATE_ON, "true") + "," + stateCmd;
			stateCmd = stateCmd.left(stateCmd.length() - 1);
		}

		uint64_t _currentTime = QDateTime::currentMSecsSinceEpoch();

		if ((_currentTime - _lastConfirm > 1500 && ((int)light.getId()) != _lastId) ||
			(_currentTime - _lastConfirm > 3000))
		{
			_lastId = light.getId();
			_lastConfirm = _currentTime;
		}

		if (!stateCmd.isEmpty())
			setLightState(light.getId(), "{" + stateCmd + "}");

		if (!powerCmd.isEmpty() && !on)
		{
			QThread::msleep(50);
			setLightState(light.getId(), "{" + powerCmd + "}");
		}
	}
}

void LedDevicePhilipsHue::setLightsCount(unsigned int lightsCount)
{
	_lightsCount = lightsCount;
}

bool LedDevicePhilipsHue::powerOn()
{
	return powerOn(true);
}

bool LedDevicePhilipsHue::powerOn(bool wait)
{
	if (_isDeviceReady)
	{
		//Switch off Philips Hue devices physically
		for (PhilipsHueLight& light : _lights)
		{
			setOnOffState(light, true, true);
		}
	}
	return true;
}

bool LedDevicePhilipsHue::powerOff()
{
	if (_isDeviceReady)
	{
		//Switch off Philips Hue devices physically
		for (PhilipsHueLight& light : _lights)
		{
			setOnOffState(light, false, true);
		}
	}
	return true;
}

bool LedDevicePhilipsHue::storeState()
{
	bool rc = true;

	if (_isRestoreOrigState)
	{
		if (!_lightIds.empty())
		{
			for (PhilipsHueLight& light : _lights)
			{
				QJsonObject values = getLightState(light.getId()).object();
				light.saveOriginalState(values);
			}
		}
	}

	return rc;
}

QJsonObject LedDevicePhilipsHue::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	
#ifdef ENABLE_BONJOUR
	auto bonInstance = BonjourBrowserWrapper::getInstance();
	if (bonInstance != nullptr)
	{
		QList<BonjourRecord> recs;
		
		if (QThread::currentThread() == bonInstance->thread())
			recs = bonInstance->getPhilipsHUE();
		else
			QMetaObject::invokeMethod(bonInstance, "getPhilipsHUE", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(QList<BonjourRecord>, recs));

		for(BonjourRecord& r : recs)
		{
			QJsonObject newIp;
			newIp["ip"] = r.address;
			newIp["port"] = (r.port == 443) ? 80: r.port;
			deviceList.push_back(newIp);
		}
	}
#else
	Error(_log, "The Network Discovery Service was mysteriously disabled while the maintenair was compiling this version of HyperHDR");
#endif
	if (deviceList.isEmpty())
	{
		// Discover Devices
		SSDPDiscover discover;

		discover.skipDuplicateKeys(false);
		discover.setSearchFilter(SSDP_FILTER, SSDP_FILTER_HEADER);
		QString searchTarget = SSDP_ID;

		if (discover.discoverServices(searchTarget) > 0)
		{
			deviceList = discover.getServicesDiscoveredJson();
		}
	}

	devicesDiscovered.insert("devices", deviceList);
	Debug(_log, "devicesDiscovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

bool LedDevicePhilipsHue::restoreState()
{
	bool rc = true;

	if (_isRestoreOrigState)
	{
		// Restore device's original state
		if (!_lightIds.empty())
		{
			for (PhilipsHueLight& light : _lights)
			{
				QString state = light.getOriginalState();
				if (!state.isEmpty())
					setLightState(light.getId(), light.getOriginalState());
			}
		}
	}

	return rc;
}

QJsonObject LedDevicePhilipsHue::getProperties(const QJsonObject& params)
{
	QJsonObject properties;

	// Get Phillips-Bridge device properties
	QString host = params["host"].toString("");
	if (!host.isEmpty())
	{
		QString username = params["user"].toString("");
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

		initRestAPI(apiHost, apiPort, username);
		_restApi->setPath(filter);

		// Perform request
		httpResponse response = _restApi->get();
		if (response.error())
		{
			Warning(_log, "%s get properties failed with error: '%s'", QSTRING_CSTR(_activeDeviceType), QSTRING_CSTR(response.getErrorReason()));
		}

		// Perform request
		properties.insert("properties", response.getBody().object());
	}
	return properties;
}

void LedDevicePhilipsHue::identify(const QJsonObject& params)
{
	Debug(_log, "params: [%s]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());
	QJsonObject properties;

	// Identify Phillips-Bridge device
	QString host = params["host"].toString("");
	if (!host.isEmpty())
	{
		QString username = params["user"].toString("");
		int lightId = params["lightId"].toInt(0);

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

		initRestAPI(apiHost, apiPort, username);

		QString resource = QString("%1/%2/%3").arg(API_LIGHTS).arg(lightId).arg(API_STATE);
		_restApi->setPath(resource);

		QString stateCmd;
		stateCmd += QString("\"%1\":%2,").arg(API_STATE_ON, API_STATE_VALUE_TRUE);
		stateCmd += QString("\"%1\":\"%2\"").arg("alert", "select");
		stateCmd = "{" + stateCmd + "}";

		// Perform request
		httpResponse response = _restApi->put(stateCmd);
		if (response.error())
		{
			Warning(_log, "%s identification failed with error: '%s'", QSTRING_CSTR(_activeDeviceType), QSTRING_CSTR(response.getErrorReason()));
		}
	}
}
