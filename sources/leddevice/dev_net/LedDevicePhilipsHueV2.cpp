// Local-HyperHDR includes
#include "LedDevicePhilipsHueV2.h"
#include <utils/QStringUtils.h>
#include <HyperhdrConfig.h>

#define RESERVED 0x00
#define VERSION_MINOR 0x00
#define COLORSPACE_XYBRI 0x01
#ifdef ENABLE_BONJOUR

#include <bonjour/bonjourbrowserwrapper.h>

#endif

#include <ssdp/SSDPDiscover.h>

#include <chrono>
#include <cmath>
#include <utility>

// Constants
namespace {

    bool verbose = false;

    // Configuration settings
    const char CONFIG_ADDRESS[] = "output";
    const char CONFIG_USERNAME[] = "username";
    const char CONFIG_CLIENTKEY[] = "clientkey";
    const char CONFIG_BRIGHTNESSFACTOR[] = "brightnessFactor";
    const char CONFIG_TRANSITIONTIME[] = "transitiontime";
    const char CONFIG_BLACK_LIGHTS_TIMEOUT[] = "blackLightsTimeout";
    const char CONFIG_ON_OFF_BLACK[] = "switchOffOnBlack";
    const char CONFIG_RESTORE_STATE[] = "restoreOriginalState";
    const char CONFIG_USE_HUE_ENTERTAINMENT_API[] = "useEntertainmentAPI";
    const char CONFIG_ENTERTAINMENT_CONFIGURATION_ID[] = "entertainmentConfigurationId";

    const char CONFIG_VERBOSE[] = "verbose";

    // Device Data elements
    const char DEV_DATA_BRIDGEID[] = "bridgeid";
    const char DEV_DATA_MODEL[] = "modelid";
    const char DEV_DATA_NAME[] = "name";
    //const char DEV_DATA_MANUFACTURER[] = "manufacturer";
    const char DEV_DATA_FIRMWAREVERSION[] = "swversion";
    const char DEV_DATA_APIVERSION[] = "apiversion";

    // Philips Hue OpenAPI URLs
    const int API_DEFAULT_PORT = 443; //Use default port per communication scheme
    const char API_BASE_PATH[] = "/clip/v2/resource";
    const char API_ROOT[] = "";
    const char API_STATE[] = "state";
    const char API_CONFIG[] = "config";
    const char API_CHANNELS[] = "channels";
    const char API_GROUPS[] = "entertainment_configuration";

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
    constexpr std::chrono::milliseconds STREAM_REFRESH_TIME{20};
    const int SSL_CIPHERSUITES[2] = {MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256, 0};

} //End of constants

bool operator==(const CiColorV2 &p1, const CiColorV2 &p2) {
    return ((p1.x == p2.x) && (p1.y == p2.y) && (p1.bri == p2.bri));
}

bool operator!=(const CiColorV2 &p1, const CiColorV2 &p2) {
    return !(p1 == p2);
}

CiColorV2 CiColorV2::rgbToCiColor(double red, double green, double blue, const CiColorTriangleV2 &colorSpace, bool candyGamma) {
    double cx;
    double cy;
    double bri;

    if (red + green + blue > 0) {
        // Apply gamma correction.
        double r = red;
        double g = green;
        double b = blue;

        if (candyGamma) {
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
    } else {
        cx = 0.0;
        cy = 0.0;
        bri = 0.0;
    }

    if (std::isnan(cx)) {
        cx = 0.0;
    }
    if (std::isnan(cy)) {
        cy = 0.0;
    }
    if (std::isnan(bri)) {
        bri = 0.0;
    }

    CiColorV2 xy = {cx, cy, bri};

    if ((red + green + blue) > 0) {
        // Check if the given XY value is within the color reach of our lamps.
        if (!isPointInLampsReach(xy, colorSpace)) {
            // It seems the color is out of reach let's find the closes color we can produce with our lamp and send this XY value out.
            ChannelColor pAB = getClosestPointToPoint(colorSpace.red, colorSpace.green, xy);
            ChannelColor pAC = getClosestPointToPoint(colorSpace.blue, colorSpace.red, xy);
            ChannelColor pBC = getClosestPointToPoint(colorSpace.green, colorSpace.blue, xy);
            // Get the distances per point and see which point is closer to our Point.
            double dAB = getDistanceBetweenTwoPoints(xy, pAB);
            double dAC = getDistanceBetweenTwoPoints(xy, pAC);
            double dBC = getDistanceBetweenTwoPoints(xy, pBC);
            double lowest = dAB;
            ChannelColor closestPoint = pAB;
            if (dAC < lowest) {
                lowest = dAC;
                closestPoint = pAC;
            }
            if (dBC < lowest) {
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

double CiColorV2::crossProduct(ChannelColor p1, ChannelColor p2) {
    return p1.x * p2.y - p1.y * p2.x;
}

bool CiColorV2::isPointInLampsReach(CiColorV2 p, const CiColorTriangleV2 &colorSpace) {
    bool rc = false;
    ChannelColor v1 = {colorSpace.green.x - colorSpace.red.x, colorSpace.green.y - colorSpace.red.y};
    ChannelColor v2 = {colorSpace.blue.x - colorSpace.red.x, colorSpace.blue.y - colorSpace.red.y};
    ChannelColor q = {p.x - colorSpace.red.x, p.y - colorSpace.red.y};
    double s = crossProduct(q, v2) / crossProduct(v1, v2);
    double t = crossProduct(v1, q) / crossProduct(v1, v2);
    if ((s >= 0.0) && (t >= 0.0) && (s + t <= 1.0)) {
        rc = true;
    }
    return rc;
}

ChannelColor CiColorV2::getClosestPointToPoint(ChannelColor a, ChannelColor b, CiColorV2 p) {
    ChannelColor AP = {p.x - a.x, p.y - a.y};
    ChannelColor AB = {b.x - a.x, b.y - a.y};
    double ab2 = AB.x * AB.x + AB.y * AB.y;
    double ap_ab = AP.x * AB.x + AP.y * AB.y;
    double t = ap_ab / ab2;
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }
    return {a.x + AB.x * t, a.y + AB.y * t};
}

double CiColorV2::getDistanceBetweenTwoPoints(CiColorV2 p1, ChannelColor p2) {
    // Horizontal difference.
    double dx = p1.x - p2.x;
    // Vertical difference.
    double dy = p1.y - p2.y;
    // Absolute value.
    return sqrt(dx * dx + dy * dy);
}

LedDevicePhilipsHueBridgeV2::LedDevicePhilipsHueBridgeV2(const QJsonObject &deviceConfig)
        : ProviderUdpSSL(deviceConfig), _restApi(nullptr), _apiPort(API_DEFAULT_PORT), _useHueEntertainmentAPI(true),
          _api_major(0), _api_minor(0), _api_patch(0), _isHueEntertainmentReady(false) {
}

LedDevicePhilipsHueBridgeV2::~LedDevicePhilipsHueBridgeV2() {
    delete _restApi;
    _restApi = nullptr;
}

bool LedDevicePhilipsHueBridgeV2::init(const QJsonObject &deviceConfig) {
    // Overwrite non supported/required features
    if (deviceConfig["refreshTime"].toInt(0) > 0) {
        InfoIf((!_useHueEntertainmentAPI), _log,
               "Device Philips Hue does not require setting refresh rate. Refresh time is ignored.");
        _devConfig["refreshTime"] = 0;
    }

    DebugIf(verbose, _log, "deviceConfig: [%s]",
            QString(QJsonDocument(_devConfig).toJson(QJsonDocument::Compact)).toUtf8().constData());

    bool isInitOK = false;

    if (LedDevice::init(deviceConfig)) {

        log("DeviceType", "%s", QSTRING_CSTR(this->getActiveDeviceType()));
        log("LedCount", "%d", this->getLedCount());
        log("RefreshTime", "%d", _refreshTimerInterval_ms);

        //Set hostname as per configuration and_defaultHost default port
        QString address = deviceConfig[CONFIG_ADDRESS].toString();

        //If host not configured the init failed
        if (address.isEmpty()) {
            this->setInError("No target hostname nor IP defined");
            isInitOK = false;
        } else {
            QStringList addressparts = QStringUtils::SPLITTER(address, ':');
            _hostname = addressparts[0];
            log("Hostname/IP", "%s", QSTRING_CSTR(_hostname));

            _username = deviceConfig[CONFIG_USERNAME].toString();

            if (initRestAPI(_hostname, _apiPort, _username)) {
                if (initMaps()) {
                    isInitOK = ProviderUdpSSL::init(_devConfig);
                } else {
                    _currentRetry = _maxRetry;
                    if (!_retryMode) {
                        _retryMode = true;
                        QTimer::singleShot(1000, [this]() {
                            _retryMode = false;
                            if (_currentRetry > 0) enableDevice(true);
                        });
                    }
                }
            }
        }
    }

    return isInitOK;
}

bool LedDevicePhilipsHueBridgeV2::initRestAPI(const QString &hostname, int port, const QString &token) {
    if (_restApi == nullptr)
        _restApi = new ProviderRestApi(hostname, port);
    else
        _restApi->updateHost(hostname, port);

    _restApi->setBasePath(QString(API_BASE_PATH) + "/");
    _restApi->addHeader("hue-application-key", token);
    return true;
}

bool LedDevicePhilipsHueBridgeV2::checkApiError(const QJsonDocument &response, bool supressError) {
    bool apiError = false;
    QString errorReason;

    QString strJson(response.toJson(QJsonDocument::Compact));
    DebugIf(verbose, _log, "Reply: [%s]", strJson.toUtf8().constData());

    QVariantList rspList = response.toVariant().toList();
    if (!rspList.isEmpty()) {
        QVariantMap map = rspList.first().toMap();
        if (map.contains(API_ERROR)) {
            // API call failed to execute an error message was returned
            QString errorAddress = map.value(API_ERROR).toMap().value(API_ERROR_ADDRESS).toString();
            QString errorDesc = map.value(API_ERROR).toMap().value(API_ERROR_DESCRIPTION).toString();
            QString errorType = map.value(API_ERROR).toMap().value(API_ERROR_TYPE).toString();

            log("Error Type", "%s", QSTRING_CSTR(errorType));
            log("Error Address", "%s", QSTRING_CSTR(errorAddress));
            log("Error Address Description", "%s", QSTRING_CSTR(errorDesc));

            if (errorType != "901") {
                errorReason = QString("(%1) %2, Resource:%3").arg(errorType, errorDesc, errorAddress);

                if (!supressError) {
                    this->setInError(errorReason);
                } else
                    Warning(_log, "Suppresing error: %s", QSTRING_CSTR(errorReason));

                apiError = true;
            }
        }
    }
    return apiError;
}

const int *LedDevicePhilipsHueBridgeV2::getCiphersuites() const {
    return SSL_CIPHERSUITES;
}

void LedDevicePhilipsHueBridgeV2::log(const char *msg, const char *type, ...) const {
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

QJsonDocument LedDevicePhilipsHueBridgeV2::getAllBridgeInfos() {
    return get(API_BASE_PATH);
}

bool LedDevicePhilipsHueBridgeV2::initMaps() {
    bool isInitOK = true;

    QJsonDocument doc = getAllBridgeInfos();
    setApplicationId();
    DebugIf(verbose, _log, "doc: [%s]",
            QString(QJsonDocument(doc).toJson(QJsonDocument::Compact)).toUtf8().constData());

    if (doc.isEmpty())
        setInError("Could not read the bridge details");

    if (this->isInError()) {
        isInitOK = false;
    } else {
        setBridgeConfig(get("/api/config"));
        setGroupMap(doc);
        setLightsMap(doc);
    }

    return isInitOK;
}

void LedDevicePhilipsHueBridgeV2::setBridgeConfig(const QJsonDocument &doc) {
    QJsonObject jsonConfigInfo = doc.object();
    if (verbose) {
        std::cout << "jsonConfigInfo: ["
                  << QString(QJsonDocument(jsonConfigInfo).toJson(QJsonDocument::Compact)).toUtf8().constData() << "]"
                  << std::endl;
    }

    QString deviceName = jsonConfigInfo[DEV_DATA_NAME].toString();
    _deviceModel = jsonConfigInfo[DEV_DATA_MODEL].toString();
    QString deviceBridgeID = jsonConfigInfo[DEV_DATA_BRIDGEID].toString();
    _deviceFirmwareVersion = jsonConfigInfo[DEV_DATA_FIRMWAREVERSION].toString();
    _deviceAPIVersion = jsonConfigInfo[DEV_DATA_APIVERSION].toString();

    QStringList apiVersionParts = QStringUtils::SPLITTER(_deviceAPIVersion, '.');
    if (!apiVersionParts.isEmpty()) {
        _api_major = apiVersionParts[0].toUInt();
        _api_minor = apiVersionParts[1].toUInt();
        _api_patch = apiVersionParts[2].toUInt();

        if (_api_major > 1 || (_api_major == 1 && _api_minor >= 22)) {
            _isHueEntertainmentReady = true;
        }
    }

    if (_useHueEntertainmentAPI) {
        DebugIf(!_isHueEntertainmentReady, _log,
                "Bridge is not Entertainment API Ready - Entertainment API usage was disabled!");
        _useHueEntertainmentAPI = _isHueEntertainmentReady;
    }

    log("Bridge Name", "%s", QSTRING_CSTR(deviceName));
    log("Model", "%s", QSTRING_CSTR(_deviceModel));
    log("Bridge-ID", "%s", QSTRING_CSTR(deviceBridgeID));
    log("SoftwareVersion", "%s", QSTRING_CSTR(_deviceFirmwareVersion));
    log("API-Version", "%u.%u.%u", _api_major, _api_minor, _api_patch);
    log("EntertainmentReady", "%d", static_cast<int>(_isHueEntertainmentReady));
}

void LedDevicePhilipsHueBridgeV2::setLightsMap(const QJsonDocument &doc) {
    QJsonArray jsonLightsInfo;
    _lightsMap.clear();
    for (const auto &item: doc.object()["data"].toArray()) {
        if (item.toObject()["type"] == "light") {
            jsonLightsInfo.append(item.toObject());
            _lightsMap.insert(item.toObject()["id_v1"].toString(), item.toObject());
        }
    };
    DebugIf(verbose, _log, "jsonLightsInfo: [%s]",
            QString(QJsonDocument(jsonLightsInfo).toJson(QJsonDocument::Compact)).toUtf8().constData());

    _ledCount = static_cast<uint>(_lightsMap.size());


    if (getLedCount() == 0) {
        this->setInError("No light-IDs found at the Philips Hue Bridge");
    } else {
        log("Lights in Bridge found", "%d", getLedCount());
    }
}

void LedDevicePhilipsHueBridgeV2::setGroupMap(const QJsonDocument &doc) {
    QJsonArray jsonGroupsInfo;
    _groupsMap.clear();
    for (const auto &item: doc.object()["data"].toArray()) {
        if (item.toObject()["type"] == "entertainment_configuration") {
            jsonGroupsInfo.append(item.toObject());
            _groupsMap.insert(item.toObject()["id"].toString(), item.toObject());
        }
    };

    DebugIf(verbose, _log, "jsonGroupsInfo: [%s]",
            QString(QJsonDocument(jsonGroupsInfo).toJson(QJsonDocument::Compact)).toUtf8().constData());
}

QMap<QString, QJsonObject> LedDevicePhilipsHueBridgeV2::getGroupMap() const {
    return _groupsMap;
}

QString LedDevicePhilipsHueBridgeV2::getGroupName(QString groupId) const {
    QString groupName;
    if (_groupsMap.contains(groupId)) {
        QJsonObject group = _groupsMap.value(groupId);
        groupName = group.value(API_GROUP_NAME).toString().trimmed().replace("\"", "");
    } else {
        Error(_log, "Group ID %u doesn't exists on this bridge", QSTRING_CSTR(groupId));
    }
    return groupName;
}

QJsonArray LedDevicePhilipsHueBridgeV2::getGroupChannels(QString groupId) const {
    QJsonArray groupLights;
    // search user groupid inside _groupsMap and create light if found
    if (_groupsMap.contains(groupId)) {
        QJsonObject group = _groupsMap.value(groupId);
        QString groupName = getGroupName(groupId);
        groupLights = group.value(API_CHANNELS).toArray();

        log("Entertainment Group found", "[%s] %s", QSTRING_CSTR(groupId), QSTRING_CSTR(groupName));
        log("Channels in Group", "%d", groupLights.size());
        Info(_log, "Entertainment Group [%s] \"%s\" with %d channels found", QSTRING_CSTR(groupId),
             QSTRING_CSTR(groupName),
             groupLights.size());
    } else {
        Error(_log, "Group ID %d doesn't exists on this bridge", QSTRING_CSTR(groupId));
    }

    return groupLights;
}


QJsonDocument LedDevicePhilipsHueBridgeV2::getLightState(QString lightId) {
    DebugIf(verbose, _log, "GetLightState [%s]", QSTRING_CSTR(lightId));
    return get(QString("%1/%2").arg("light").arg(lightId));
}

void LedDevicePhilipsHueBridgeV2::setLightState(QString lightId, const QString &state) {
    DebugIf(verbose, _log, "SetLightState [%s]: %s", QSTRING_CSTR(lightId), QSTRING_CSTR(state));
    post(QString("%1/%2").arg("light").arg(lightId), state);
}

QJsonDocument LedDevicePhilipsHueBridgeV2::getGroupState(QString groupId) {
    DebugIf(verbose, _log, "GetGroupState [%s]", QSTRING_CSTR(groupId));
    return get(QString("%1/%2").arg(API_GROUPS).arg(groupId));
}

QJsonDocument LedDevicePhilipsHueBridgeV2::setGroupState(QString groupId, bool state) {
    return post(QString("%1/%2").arg(API_GROUPS).arg(groupId),
                QString(R"({"action":"%1"})").arg(state ? "start" : "stop"), true);
}

void LedDevicePhilipsHueBridgeV2::setApplicationId() {
    httpResponse res = getRaw(QString("/auth/v1"));
    _hueApplicationId = res.getHeaders().value("hue-application-id");
    Debug(_log, "Application ID is %s", QSTRING_CSTR(_hueApplicationId));
}

QJsonDocument LedDevicePhilipsHueBridgeV2::get(const QString &route) {
    if (_restApi == nullptr)
        return httpResponse().getBody();

    _restApi->setPath(route);

    httpResponse response = _restApi->get();

    checkApiError(response.getBody());
    return response.getBody();
}


httpResponse LedDevicePhilipsHueBridgeV2::getRaw(const QString &route) {
    if (_restApi == nullptr)
        return httpResponse();

    _restApi->setPath(route);

    httpResponse response = _restApi->get();
    return response;
}

QJsonDocument LedDevicePhilipsHueBridgeV2::post(const QString &route, const QString &content, bool supressError) {
    _restApi->setPath(route);

    httpResponse response = _restApi->put(content);
    checkApiError(response.getBody(), supressError);
    return response.getBody();
}

bool LedDevicePhilipsHueBridgeV2::isStreamOwner(const QString &streamOwner) const {
    return (streamOwner != "" && streamOwner == _hueApplicationId);
}

PhilipsHueChannel::PhilipsHueChannel(Logger *log, unsigned int id, QJsonObject values, QStringList lightIds,
                                     unsigned int ledidx)
        : _log(log), _id(id), _ledidx(ledidx), _on(false), _transitionTime(0), _color({0.0, 0.0, 0.0}),
          _hasColor(false),
          _colorBlack({0.0, 0.0, 0.0}), _modelId(""),
          _lastSendColor(0), _lastBlack(-1), _lastWhite(-1),
          _lightIds(std::move(lightIds)) {

    _colorSpace.red = {1.0, 0.0};
    _colorSpace.green = {0.0, 1.0};
    _colorSpace.blue = {0.0, 0.0};
    _colorBlack = {0.0, 0.0, 0.0};

    _lightname = values["name"].toString().trimmed().replace("\"", "");
    Info(_log, "Channel ID %d (\"%s\", LED index \"%d\") created",
         id, QSTRING_CSTR(_lightname), ledidx);
}

bool PhilipsHueChannel::isBusy() {
    bool temp = true;

    uint64_t _currentTime = InternalClock::now();
    if ((int64_t) (_currentTime - _lastSendColor) >= (int64_t) 100) {
        _lastSendColor = _currentTime;
        temp = false;
    }

    return temp;
}

void PhilipsHueChannel::setBlack() {
    CiColorV2 black;
    black.bri = 0;
    black.x = 0;
    black.y = 0;
    setColor(black);
}

PhilipsHueChannel::~PhilipsHueChannel() {
    DebugIf(verbose, _log, "Light ID %d (\"%s\", LED index \"%d\") deconstructed", _id, QSTRING_CSTR(_lightname),
            _ledidx);
}

unsigned int PhilipsHueChannel::getId() const {
    return _id;
}

QStringList PhilipsHueChannel::getLightIds() const {
    return _lightIds;
}

void PhilipsHueChannel::setOnOffState(bool on) {
    this->_on = on;
}

void PhilipsHueChannel::setTransitionTime(int transitionTime) {
    this->_transitionTime = transitionTime;
}

void PhilipsHueChannel::setColor(const CiColorV2 &color) {
    this->_hasColor = true;
    this->_color = color;
}

bool PhilipsHueChannel::getOnOffState() const {
    return _on;
}

int PhilipsHueChannel::getTransitionTime() const {
    return _transitionTime;
}

CiColorV2 PhilipsHueChannel::getColor() const {
    return _color;
}

ColorRgb PhilipsHueChannel::getRGBColor() const {
    return _colorRgb;
}

void PhilipsHueChannel::setRGBColor(const ColorRgb color) {
    _colorRgb = color;
}

bool PhilipsHueChannel::hasColor() {
    return _hasColor;
}

CiColorTriangleV2 PhilipsHueChannel::getColorSpace() const {
    return _colorSpace;
}

LedDevicePhilipsHueV2::LedDevicePhilipsHueV2(const QJsonObject &deviceConfig)
        : LedDevicePhilipsHueBridgeV2(deviceConfig), _switchOffOnBlack(false), _brightnessFactor(1.0),
          _transitionTime(1), _isInitLeds(false), _lightsCount(0), _entertainmentConfigurationId(""), _blackLightsTimeout(15000),
          _blackLevel(0.0), _candyGamma(true),
          _handshake_timeout_min(600), _handshake_timeout_max(2000), _stopConnection(false), _lastConfirm(0),
          _lastId(-1), _groupStreamState(false) {
}

LedDevice *LedDevicePhilipsHueV2::construct(const QJsonObject &deviceConfig) {
    return new LedDevicePhilipsHueV2(deviceConfig);
}

LedDevicePhilipsHueV2::~LedDevicePhilipsHueV2() {
}

bool LedDevicePhilipsHueV2::setLights() {
    bool isInitOK = true;

    QJsonArray lArray;

    if (_useHueEntertainmentAPI && !_entertainmentConfigurationId.isEmpty()) {
        lArray = getGroupChannels(_entertainmentConfigurationId);
    }

    if (lArray.empty()) {
        if (_useHueEntertainmentAPI) {
            _useHueEntertainmentAPI = false;
            Error(_log, "Group-ID [%s] is not usable - Entertainment API usage was disabled!", QSTRING_CSTR(_entertainmentConfigurationId));
        }
    }

    QString lightIDStr;
    _channels.clear();
    if (!lArray.empty()) {
        unsigned int ledidx = 0;
        for (const QJsonValueRef channel: lArray) {
            _channels.emplace_back(_log, channel.toObject()["channel_id"].toInt(), channel.toObject(),
                                   getLightIdsInChannel(channel.toObject()), ledidx);
            ledidx++;
            if (!lightIDStr.isEmpty()) {
                lightIDStr.append(", ");
            }
            lightIDStr.append(QString::number(channel.toObject()["channel_id"].toInt()));
        }
    }

    unsigned int configuredLightsCount = static_cast<unsigned int>(_channels.size());
    _lightsCount = configuredLightsCount;

    log("Light-IDs configured", "%d", configuredLightsCount);

    if (configuredLightsCount == 0) {
        this->setInError("No light-IDs configured");
        Error(_log, "No usable lights found!");
        isInitOK = false;
    } else {
        log("Light-IDs", "%s", QSTRING_CSTR(lightIDStr));
    }

    return isInitOK;
}


bool LedDevicePhilipsHueV2::initLeds() {
    bool isInitOK = false;

    if (!this->isInError()) {
        if (setLights()) {
            if (_useHueEntertainmentAPI) {
                _groupName = getGroupName(_entertainmentConfigurationId);
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

            } else {
                // adapt refreshTime to count of user lightIds (bridge 10Hz max overall)
                setRefreshTime(static_cast<int>(100 * getLightsCount()));
                isInitOK = true;
            }
            _isInitLeds = true;
        } else {
            isInitOK = false;
        }
    }

    return isInitOK;
}

bool LedDevicePhilipsHueV2::init(const QJsonObject &deviceConfig) {
    _configBackup = deviceConfig;

    verbose = deviceConfig[CONFIG_VERBOSE].toBool(false);

    // Initialise LedDevice configuration and execution environment
    _switchOffOnBlack = _devConfig[CONFIG_ON_OFF_BLACK].toBool(true);
    _blackLightsTimeout = _devConfig[CONFIG_BLACK_LIGHTS_TIMEOUT].toInt(15000);
    _brightnessFactor = _devConfig[CONFIG_BRIGHTNESSFACTOR].toDouble(1.0);
    _transitionTime = _devConfig[CONFIG_TRANSITIONTIME].toInt(1);
    _isRestoreOrigState = _devConfig[CONFIG_RESTORE_STATE].toBool(true);
    _entertainmentConfigurationId = _devConfig[CONFIG_ENTERTAINMENT_CONFIGURATION_ID].toString("");
    _blackLevel = _devConfig["blackLevel"].toDouble(0.0);
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
    log("SSL Handshake min", "%d", _handshake_timeout_min);
    log("SSL Handshake max", "%d", _handshake_timeout_max);

    if (_useHueEntertainmentAPI) {
        log("Entertainment API Group-ID", "%s", QSTRING_CSTR(_entertainmentConfigurationId));

        if (_entertainmentConfigurationId.isEmpty()) {
            Error(_log, "Disabling usage of HueEntertainmentAPI: Group-ID is invalid", "%d", QSTRING_CSTR(_entertainmentConfigurationId));
            _useHueEntertainmentAPI = false;
        }
    }

    bool isInitOK = LedDevicePhilipsHueBridgeV2::init(deviceConfig);

    if (isInitOK)
        isInitOK = initLeds();


    return isInitOK;
}

bool LedDevicePhilipsHueV2::openStream() {
    bool isInitOK = false;
    bool streamState = getStreamGroupState();

    if (!this->isInError()) {
        // stream is already active
        if (streamState) {

            // if same owner stop stream
            if (isStreamOwner(_streamOwner)) {
                Debug(_log, "Group: \"%s\" [%u] is in use, try to stop stream", QSTRING_CSTR(_groupName),
                      QSTRING_CSTR(_entertainmentConfigurationId));

                if (stopStream()) {
                    Debug(_log, "Stream successful stopped");
                    //Restore Philips Hue devices state
                    restoreState();
                    isInitOK = startStream();
                } else {
                    Error(_log, "Group: \"%s\" [%s] couldn't stop by user: \"%s\" - Entertainment API not usable",
                          QSTRING_CSTR(_groupName), QSTRING_CSTR(_entertainmentConfigurationId), QSTRING_CSTR(_streamOwner));
                }
            } else {
                Error(_log,
                      "Group: \"%s\" [%s] is in use and owned by other user: \"%s\" - Entertainment API not usable",
                      QSTRING_CSTR(_groupName), QSTRING_CSTR(_entertainmentConfigurationId), QSTRING_CSTR(_streamOwner));
            }
        } else {
            isInitOK = startStream();
        }
        if (isInitOK)
            storeState();
    }

    if (isInitOK) {
        // open UDP SSL Connection
        isInitOK = ProviderUdpSSL::initNetwork();

        if (isInitOK) {
            Info(_log, "Philips Hue Entertaiment API successful connected! Start Streaming.");
        } else {
            Error(_log, "Philips Hue Entertaiment API not connected!");
        }
    } else {
        Error(_log, "Philips Hue Entertaiment API could not initialisized!");
    }

    return isInitOK;
}


bool LedDevicePhilipsHueV2::startStream() {
    bool rc = true;

    Debug(_log, "Start entertainment stream");

    rc = setStreamGroupState(true);

    if (rc) {
        Debug(_log, "The stream has started.");
        _sequenceNumber = 0;

    } else
        Error(_log, "The stream has NOT started.");

    return rc;
}

bool LedDevicePhilipsHueV2::stopStream() {
    bool canRestore = false;

    // Set to black
    if (!_channels.empty() && _isDeviceReady && !_stopConnection && _useHueEntertainmentAPI) {
        if (!_isRestoreOrigState) {
            for (PhilipsHueChannel &light: _channels) {
                light.setBlack();
            }

            for (int i = 0; i < 2; i++) {
                writeStream(true);
                QThread::msleep(5);
            }

            QThread::msleep(25);
        } else
            canRestore = true;
    }

    ProviderUdpSSL::closeNetwork();

    int index = 3;
    while (!setStreamGroupState(false) && --index > 0) {
        Debug(_log, "Stop entertainment stream. Retrying...");
        QThread::msleep(500);
    }

    if (canRestore && _isRestoreOrigState)
        restoreState();

    bool rc = (index > 0);

    if (rc)
        Debug(_log, "The stream has stopped");
    else
        Error(_log, "The stream has NOT stopped.");

    return rc;
}

bool LedDevicePhilipsHueV2::setStreamGroupState(bool state) {
    QJsonDocument doc = setGroupState(_entertainmentConfigurationId, state);

    _groupStreamState = (getStreamGroupState() && isStreamOwner(_streamOwner));
    return _groupStreamState == state;
}

bool LedDevicePhilipsHueV2::getStreamGroupState() {
    QJsonObject doc = getGroupState(_entertainmentConfigurationId)["data"].toArray().first().toObject();

    if (!this->isInError()) {
        QString status = doc["status"].toString();
        log("Group state  ", "%s", QString(QJsonDocument(doc).toJson(QJsonDocument::Compact)).toUtf8().constData());
        log("Stream status ", "%s", QSTRING_CSTR(status));
        QJsonObject obj = doc["active_streamer"].toObject();

        if (status.isEmpty()) {
            this->setInError("no Streaming Infos in Group found");
            return false;
        } else {
            _streamOwner = obj.value("rid").toString();
            bool streamState = status == "active";
            return streamState;
        }
    }

    return false;
}

int LedDevicePhilipsHueV2::close() {
    int retval = 0;

    _isDeviceReady = false;

    return retval;
}

void LedDevicePhilipsHueV2::stop() {
    _currentRetry = 0;

    LedDevicePhilipsHueBridgeV2::stop();
}

int LedDevicePhilipsHueV2::open() {
    int retval = 0;

    _isDeviceReady = true;

    return retval;
}


bool LedDevicePhilipsHueV2::switchOn() {
    bool rc = false;

    if (_retryMode)
        return false;

    Info(_log, "Switching ON Philips Hue device");

    try {
        if (_isOn) {
            _currentRetry = 0;

            Debug(_log, "Philips is already enabled. Skipping.");
            rc = true;
        } else {
            if (!_isDeviceInitialised && initMaps()) {
                init(_configBackup);
                _currentRetry = 0;
                _isDeviceInitialised = true;
            }

            if (_isDeviceInitialised) {
                if (_useHueEntertainmentAPI) {
                    if (openStream()) {
                        _isOn = true;
                        rc = true;
                    }
                } else {
                    storeState();
                    if (powerOn()) {
                        _isOn = true;
                        rc = true;
                    }
                }
            }

            if (!_isOn && _maxRetry > 0) {
                if (_currentRetry <= 0)
                    _currentRetry = _maxRetry + 1;

                _currentRetry--;

                if (_currentRetry > 0)
                    Warning(_log, "The PhilipsHue device is not ready... will try to reconnect (try %i/%i).",
                            (_maxRetry - _currentRetry + 1), _maxRetry);
                else
                    Error(_log, "The PhilipsHue device is not ready... give up.");


                if (_currentRetry > 0) {
                    _retryMode = true;
                    QTimer::singleShot(1000, [this]() {
                        _retryMode = false;
                        if (_currentRetry > 0) enableDevice(true);
                    });
                }
            } else
                _currentRetry = 0;
        }
    }
    catch (...) {
        rc = false;
        Error(_log, "Philips Hue error detected (ON)");
    }

    if (rc && _isOn)
        Info(_log, "Philips Hue device is ON");
    else
        Warning(_log, "Could not set Philips Hue device ON");

    return rc;
}

bool LedDevicePhilipsHueV2::switchOff() {
    bool result = false;

    Info(_log, "Switching OFF Philips Hue device");

    _currentRetry = 0;

    try {
        if (_useHueEntertainmentAPI && _groupStreamState) {
            result = stopStream();
            _isOn = false;
        } else
            result = LedDevicePhilipsHueBridgeV2::switchOff();
    }
    catch (...) {
        result = false;
        Error(_log, "Philips Hue error detected (OFF)");
    }

    if (result && !_isOn)
        Info(_log, "Philips Hue device is OFF");
    else
        Warning(_log, "Could not set Philips Hue device OFF");

    return result;
}

int LedDevicePhilipsHueV2::write(const std::vector<ColorRgb> &ledValues) {
    // lights will be empty sometimes
    if (_channels.empty()) {
        return -1;
    }

    // more lights then leds, stop always
    if (ledValues.size() < getLightsCount()) {
        Error(_log, "More light-IDs configured than leds, each light-ID requires one led!");
        return -1;
    }

    writeSingleLights(ledValues);

    if (_useHueEntertainmentAPI && _isInitLeds) {
        writeStream();
    }

    return 0;
}

int LedDevicePhilipsHueV2::writeSingleLights(const std::vector<ColorRgb> &ledValues) {
    // Iterate through lights and set colors.
    unsigned int idx = 0;
    for (PhilipsHueChannel &light: _channels) {
        // Get color.
        ColorRgb color;
        if (idx > ledValues.size() - 1) {
            color.red = 0;
            color.green = 0;
            color.blue = 0;
        } else {
            color = ledValues.at(idx);
        }

        light.setRGBColor(color);
        // Scale colors from [0, 255] to [0, 1] and convert to xy space.
        CiColorV2 xy = CiColorV2::rgbToCiColor(color.red / 255.0, color.green / 255.0, color.blue / 255.0,
                                               light.getColorSpace(), _candyGamma);

        this->setOnOffState(light, true);
        this->setColor(light, xy);
        idx++;
    }

    return 0;
}

void LedDevicePhilipsHueV2::writeStream(bool flush) {
    QString entertainmentConfigId = getGroupMap().value(_entertainmentConfigurationId)["id"].toString();
    std::vector<uint8_t> payload;

    payload.push_back(static_cast<uint8_t>('H'));
    payload.push_back(static_cast<uint8_t>('u'));
    payload.push_back(static_cast<uint8_t>('e'));
    payload.push_back(static_cast<uint8_t>('S'));
    payload.push_back(static_cast<uint8_t>('t'));
    payload.push_back(static_cast<uint8_t>('r'));
    payload.push_back(static_cast<uint8_t>('e'));
    payload.push_back(static_cast<uint8_t>('a'));
    payload.push_back(static_cast<uint8_t>('m'));
    payload.push_back(static_cast<uint8_t>(0x02));
    payload.push_back(static_cast<uint8_t>(VERSION_MINOR));
    payload.push_back(static_cast<uint8_t>(_sequenceNumber++));
    payload.push_back(static_cast<uint8_t>(RESERVED));
    payload.push_back(static_cast<uint8_t>(RESERVED));
    payload.push_back(static_cast<uint8_t>(COLORSPACE_XYBRI));
    payload.push_back(static_cast<uint8_t>(RESERVED));
    for (int i = 0; i < entertainmentConfigId.size(); ++i) {
        payload.push_back(static_cast<uint8_t>(entertainmentConfigId.at(i).toLatin1()));
    }
    for (auto light: _channels) {
        auto id = static_cast<uint8_t>(light.getId() & 0x00ff);

        CiColorV2 lightXY = light.getColor();
        ColorRgb lightRGB = light.getRGBColor();
        payload.push_back(id);
        quint64 R = lightXY.x * 0xffff;
        quint64 G = lightXY.y * 0xffff;
        quint64 B = lightXY.bri * 0xffff;

        payload.push_back(static_cast<uint8_t>((R >> 8) & 0xff));
        payload.push_back(static_cast<uint8_t>(R & 0xff));
        payload.push_back(static_cast<uint8_t>((G >> 8) & 0xff));
        payload.push_back(static_cast<uint8_t>(G & 0xff));
        payload.push_back(static_cast<uint8_t>((B >> 8) & 0xff));
        payload.push_back(static_cast<uint8_t>(B & 0xff));


    }
    writeBytes(payload.size(), reinterpret_cast<unsigned char *>(payload.data()), flush);
}

void LedDevicePhilipsHueV2::setOnOffState(PhilipsHueChannel &light, bool on, bool force) {
    if (light.getOnOffState() != on || force) {
        light.setOnOffState(on);
        QString state = on ? API_STATE_VALUE_TRUE : API_STATE_VALUE_FALSE;
        for (const auto &item: light.getLightIds()) {
            setLightState(item, QString(R"({"on":{"%1": %2 }})").arg(API_STATE_ON, state));
        }
    }
}

void LedDevicePhilipsHueV2::setColor(PhilipsHueChannel &light, CiColorV2 &color) {
    if (!light.hasColor() || light.getColor() != color) {
        color.bri = (qMin((double) 1.0, _brightnessFactor * qMax((double) 0, color.bri)));
        light.setColor(color);
    }
}

void LedDevicePhilipsHueV2::setState(PhilipsHueChannel &light, bool on, const CiColorV2 &color) {
    QString stateCmd, powerCmd;;
    bool priority = false;

    if (light.getOnOffState() != on) {
        light.setOnOffState(on);
        QString state = on ? API_STATE_VALUE_TRUE : API_STATE_VALUE_FALSE;
        powerCmd = QString(R"("on":{"%1":%2})").arg(API_STATE_ON, state);
        priority = true;
    }

    if (light.getTransitionTime() != _transitionTime) {
        light.setTransitionTime(_transitionTime);
        stateCmd += QString("\"%1\":%2,").arg(API_TRANSITIONTIME).arg(_transitionTime);
    }

    const int bri = qRound(qMin(254.0, _brightnessFactor * qMax(1.0, color.bri * 254.0)));
    if (!light.hasColor() || light.getColor() != color) {
        if (!light.isBusy() || priority) {
            light.setColor(color);
            stateCmd += QString(R"("color":{"%1":{"x":%2,"y":%3}},"dimming":{"%4":%5},)").arg(API_XY_COORDINATES).arg(
                    color.x, 0, 'd', 4).arg(color.y, 0, 'd', 4).arg(API_BRIGHTNESS).arg(bri);
        }
    }

    if (!stateCmd.isEmpty() || !powerCmd.isEmpty()) {
        if (!stateCmd.isEmpty()) {
            stateCmd = QString(R"("on":{"%1":%2})").arg(API_STATE_ON, "true") + "," + stateCmd;
            stateCmd = stateCmd.left(stateCmd.length() - 1);
        }

        uint64_t _currentTime = InternalClock::now();

        if ((_currentTime - _lastConfirm > 1500 && ((int) light.getId()) != _lastId) ||
            (_currentTime - _lastConfirm > 3000)) {
            _lastId = light.getId();
            _lastConfirm = _currentTime;
        }

        if (!stateCmd.isEmpty())
            for (const auto &item: light.getLightIds()) {
                setLightState(item, "{" + stateCmd + "}");
            }

        if (!powerCmd.isEmpty() && !on) {
            QThread::msleep(50);
            for (const auto &item: light.getLightIds()) {
                setLightState(item, "{" + powerCmd + "}");
            }
        }
    }
}

bool LedDevicePhilipsHueV2::powerOn() {
    return powerOn(true);
}

bool LedDevicePhilipsHueV2::powerOn(bool wait) {
    if (_isDeviceReady) {
        //Switch off Philips Hue devices physically
        for (PhilipsHueChannel &light: _channels) {
            setOnOffState(light, true, true);
        }
    }
    return true;
}

bool LedDevicePhilipsHueV2::powerOff() {
    if (_isDeviceReady) {
        //Switch off Philips Hue devices physically
        for (PhilipsHueChannel &light: _channels) {
            setOnOffState(light, false, true);
        }
    }
    return true;
}

bool LedDevicePhilipsHueV2::storeState() {
    bool rc = true;

    if (_isRestoreOrigState) {
        QMap<QString, QJsonObject> lightStateMap = getLightStateMap();
        if (!lightStateMap.empty()) {
            for (auto it = lightStateMap.begin(); it != lightStateMap.end(); ++it) {
                lightStateMap[it.key()] = getLightState(it.key()).object()["data"].toArray().first().toObject();
            }
        }
    }

    return rc;
}

QJsonObject LedDevicePhilipsHueV2::discover(const QJsonObject & /*params*/) {
    QJsonObject devicesDiscovered;
    QJsonArray deviceList;
    devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

#ifdef ENABLE_BONJOUR
    auto bonInstance = BonjourBrowserWrapper::getInstance();
    if (bonInstance != nullptr) {
        QList<BonjourRecord> recs;

        if (QThread::currentThread() == bonInstance->thread())
            recs = bonInstance->getPhilipsHUE();
        else
            QMetaObject::invokeMethod(bonInstance, "getPhilipsHUE", Qt::ConnectionType::BlockingQueuedConnection,
                                      Q_RETURN_ARG(QList<BonjourRecord>, recs));

        for (BonjourRecord &r: recs) {
            QJsonObject newIp;
            newIp["ip"] = r.address;
            newIp["port"] = (r.port == 443) ? 80 : r.port;
            deviceList.push_back(newIp);
        }
    }
#else
    Error(_log, "The Network Discovery Service was mysteriously disabled while the maintenair was compiling this version of HyperHDR");
#endif
    if (deviceList.isEmpty()) {
        // Discover Devices
        SSDPDiscover discover;

        discover.skipDuplicateKeys(false);
        discover.setSearchFilter(SSDP_FILTER, SSDP_FILTER_HEADER);
        QString searchTarget = SSDP_ID;

        if (discover.discoverServices(searchTarget) > 0) {
            deviceList = discover.getServicesDiscoveredJson();
        }
    }

    devicesDiscovered.insert("devices", deviceList);
    Debug(_log, "devicesDiscovered: [%s]",
          QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

    return devicesDiscovered;
}

bool LedDevicePhilipsHueV2::restoreState() {
    bool rc = true;

    if (_isRestoreOrigState) {
        // Restore device's original state
        if (!_channels.empty()) {
            QMap<QString, QJsonObject> lightStateMap = getLightStateMap();
            for (auto it = lightStateMap.begin(); it != lightStateMap.end(); ++it) {
                setLightState(it.key(), QJsonDocument(it.value()).toJson(QJsonDocument::JsonFormat::Compact).trimmed());
            }
        }
    }

    return rc;
}

QJsonObject LedDevicePhilipsHueV2::getProperties(const QJsonObject &params) {
    QJsonObject properties;

    // Get Phillips-Bridge device properties
    QString host = params["host"].toString("");
    if (!host.isEmpty()) {
        QString username = params["user"].toString("");
        QString filter = params["filter"].toString("");

        // Resolve hostname and port (or use default API port)
        QStringList addressparts = QStringUtils::SPLITTER(host, ':');
        QString apiHost = addressparts[0];
        int apiPort;

        if (addressparts.size() > 1) {
            apiPort = addressparts[1].toInt();
        } else {
            apiPort = API_DEFAULT_PORT;
        }

        initRestAPI(apiHost, apiPort, username);
        _restApi->setPath(filter);

        // Perform request
        httpResponse response = _restApi->get();
        if (response.error()) {
            Warning(_log, "%s get properties failed with error: '%s'", QSTRING_CSTR(_activeDeviceType),
                    QSTRING_CSTR(response.getErrorReason()));
        }

        // Perform request
        properties.insert("properties", response.getBody().object());
    }
    return properties;
}

void LedDevicePhilipsHueV2::identify(const QJsonObject &params) {
    Debug(_log, "params: [%s]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());
    QJsonObject properties;

    // Identify Phillips-Bridge device
    QString host = params["host"].toString("");
    if (!host.isEmpty()) {
        QString username = params["user"].toString("");
        QString lightId = params["lightId"].toString("");
        QString deviceId = params["deviceId"].toString("");

        // Resolve hostname and port (or use default API port)
        QStringList addressparts = QStringUtils::SPLITTER(host, ':');
        QString apiHost = addressparts[0];
        int apiPort;
        apiPort = API_DEFAULT_PORT;

        initRestAPI(apiHost, apiPort, username);

        QString resource = QString("%1/%2").arg("device").arg(deviceId);
        _restApi->setPath(resource);

        QString stateCmd;
//        stateCmd += QString(R"("%1":{"%1":true},)").arg(API_STATE_ON);
        stateCmd += QString(R"("%1":{"%2":"%3"})").arg("identify", "action", "identify");
        stateCmd = "{" + stateCmd + "}";

        // Perform request
        httpResponse response = _restApi->put(stateCmd);
        if (response.error()) {
            Warning(_log, "%s identification failed with error: '%s'", QSTRING_CSTR(_activeDeviceType),
                    QSTRING_CSTR(response.getErrorReason()));
        }
    }
}

QMap<QString, QJsonObject> LedDevicePhilipsHueBridgeV2::getLightStateMap() {
    return _lightStateMap;
}

QStringList LedDevicePhilipsHueBridgeV2::getLightIdsInChannel(QJsonObject channel) {
    QStringList lightIDS;
    for (const auto &item: channel["members"].toArray()) {
        QJsonObject service = item.toObject()["service"].toObject();
        if (service["rtype"].toString() == "entertainment") {
            QJsonDocument jsonDocument = get(QString("%1/%2").arg("entertainment", service["rid"].toString()));
            QJsonObject entertainment = jsonDocument.object()["data"].toArray().first().toObject();
            QString ownerType = entertainment["owner"].toObject()["rtype"].toString();
            if (ownerType == "device") {
                QJsonDocument deviceDocument = get(
                        QString("%1/%2").arg("device", entertainment["owner"].toObject()["rid"].toString()));
                QJsonObject device = deviceDocument.object()["data"].toArray().first().toObject();
                for (const auto &item: device["services"].toArray()) {
                    if (item.toObject()["rtype"].toString() == "light") {
                        const QString &lightId = item.toObject()["rid"].toString();
                        lightIDS.append(lightId);
                        _lightStateMap.insert(lightId, QJsonObject());
                    }
                }
            }
        }
    }
    return lightIDS;
}
