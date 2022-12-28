#pragma once

// STL includes
#include <set>
#include <string>
#include <stdarg.h>

// Qt includes
#include <QStringList>

// LedDevice includes
#include <leddevice/LedDevice.h>
#include "ProviderRestApi.h"
#include "ProviderUdpSSL.h"

/**
 * A XY color point in the color space of the hue system without brightness.
 */
struct ChannelColor {
    /// X component.
    double x;
    /// Y component.
    double y;
};

/**
 * Color triangle to define an available color space for the hue lamps.
 */
struct CiColorTriangleV2 {
    ChannelColor red, green, blue;
};

/**
 * A color point in the color space of the hue system.
 */
struct CiColorV2 {
    /// X component.
    double x;
    /// Y component.
    double y;
    /// The brightness.
    double bri;

    ///
    /// Converts an RGB color to the Hue xy color space and brightness.
    /// https://github.com/PhilipsHue/PhilipsHueSDK-iOS-OSX/blob/master/ApplicationDesignNotes/RGB%20to%20xy%20Color%20conversion.md
    ///
    /// @param red the red component in [0, 1]
    ///
    /// @param green the green component in [0, 1]
    ///
    /// @param blue the blue component in [0, 1]
    ///
    /// @return color point
    ///
    static CiColorV2
    rgbToCiColor(double red, double green, double blue, const CiColorTriangleV2 &colorSpace, bool candyGamma);

    ///
    /// @param p the color point to check
    ///
    /// @return true if the color point is covered by the lamp color space
    ///
    static bool isPointInLampsReach(CiColorV2 p, const CiColorTriangleV2 &colorSpace);

    ///
    /// @param p1 point one
    ///
    /// @param p2 point tow
    ///
    /// @return the cross product between p1 and p2
    ///
    static double crossProduct(ChannelColor p1, ChannelColor p2);

    ///
    /// @param a reference point one
    ///
    /// @param b reference point two
    ///
    /// @param p the point to which the closest point is to be found
    ///
    /// @return the closest color point of p to a and b
    ///
    static ChannelColor getClosestPointToPoint(ChannelColor a, ChannelColor b, CiColorV2 p);

    ///
    /// @param p1 point one
    ///
    /// @param p2 point tow
    ///
    /// @return the distance between the two points
    ///
    static double getDistanceBetweenTwoPoints(CiColorV2 p1, ChannelColor p2);
};

bool operator==(const CiColorV2 &p1, const CiColorV2 &p2);

bool operator!=(const CiColorV2 &p1, const CiColorV2 &p2);

/**
 * Simple class to hold the id, the latest color, the color space and the original state.
 */
class PhilipsHueChannel {

public:
    ///
    /// Constructs the light.
    ///
    /// @param log the logger
    /// @param bridge the bridge
    /// @param id the light id
    ///
    PhilipsHueChannel(Logger *log,
                      unsigned int id, QJsonObject values,QStringList lightIds, unsigned int ledidx);

    ~PhilipsHueChannel();

    ///
    /// @param on
    ///
    void setOnOffState(bool on);

    ///
    /// @param transitionTime the transition time between colors in multiples of 100 ms
    ///
    void setTransitionTime(int transitionTime);

    ///
    /// @param color the color to set
    ///
    void setColor(const CiColorV2 &color);

    unsigned int getId() const;

    bool getOnOffState() const;

    int getTransitionTime() const;

    CiColorV2 getColor() const;
    ColorRgb getRGBColor() const;
    void setRGBColor(const ColorRgb color);

    bool hasColor();

    ///
    /// @return the color space of the light determined by the model id reported by the bridge.
    CiColorTriangleV2 getColorSpace() const;

    QString getOriginalState() const;

    bool isBusy();

    bool isBlack(bool isBlack);

    bool isWhite(bool isWhite);

    void setBlack();

    void blackScreenTriggered();
    QStringList getLightIds() const;

private:

    Logger *_log;
    /// light id
    unsigned int _id;
    unsigned int _ledidx;
    bool _on;
    int _transitionTime;
    CiColorV2 _color;
    ColorRgb _colorRgb;
    bool _hasColor;
    /// darkes blue color in hue lamp GAMUT = black
    CiColorV2 _colorBlack;
    /// The model id of the hue lamp which is used to determine the color space.
    QString _modelId;
    QString _lightname;
    CiColorTriangleV2 _colorSpace;

    /// The json string of the original state.
    QJsonObject _originalStateJSON;

    CiColorV2 _originalColor;
    uint64_t _lastSendColor;
    uint64_t _lastBlack;
    uint64_t _lastWhite;
    QStringList _lightIds;

};

class LedDevicePhilipsHueBridgeV2 : public ProviderUdpSSL {
Q_OBJECT

public:

    explicit LedDevicePhilipsHueBridgeV2(const QJsonObject &deviceConfig);

    ~LedDevicePhilipsHueBridgeV2() override;

    ///
    /// @brief Initialise the access to the REST-API wrapper
    ///
    /// @param[in] host
    /// @param[in] port
    /// @param[in] authentication token
    ///
    /// @return True, if success
    ///
    bool initRestAPI(const QString &hostname, int port, const QString &token);

    ///
    /// @brief Perform a REST-API GET
    ///
    /// @param route the route of the GET request.
    ///
    /// @return the content of the GET request.
    ///
    QJsonDocument get(const QString &route);

    ///
    /// @brief Perform a REST-API POST
    ///
    /// @param route the route of the POST request.
    /// @param content the content of the POST request.
    ///
    QJsonDocument post(const QString &route, const QString &content, bool supressError = false);

    QJsonDocument getLightState(QString lightId);

    void setLightState(QString lightId = "", const QString &state = "");

    QMap<QString, QJsonObject> getLightMap() const;

    QMap<QString, QJsonObject> getGroupMap() const;

    QString getGroupName(QString groupId = "0") const;

    QJsonArray getGroupChannels(QString groupId = 0) const;

protected:

    ///
    /// @brief Initialise the Hue-Bridge configuration and network address details
    ///
    /// @param[in] deviceConfig the JSON device configuration
    /// @return True, if success
    ///
    bool init(const QJsonObject &deviceConfig) override;

    ///
    /// @brief Check, if Hue API response indicate error
    ///
    /// @param[in] response from Hue-Bridge in JSON-format
    /// return True, Hue Bridge reports error
    ///
    bool checkApiError(const QJsonDocument &response, bool supressError = false);

    ///REST-API wrapper
    ProviderRestApi *_restApi;

    ///REST-API V1 wrapper
    ProviderRestApi *_restApiV1;

    /// Ip address of the bridge
    QString _hostname;
    int _apiPort;
    /// User name for the API ("newdeveloper")
    QString _username;

    bool _useHueEntertainmentAPI;
    std::atomic<uint8_t> _sequenceNumber;

    QJsonDocument getGroupState(QString groupId);

    QJsonDocument setGroupState(QString groupId, bool state);

    bool isStreamOwner(const QString &streamOwner) const;

    bool initMaps();

    QStringList getLightIdsInChannel(QJsonObject channel);
    QMap<QString,QJsonObject> getLightStateMap();
    QMap<QString,QJsonObject> getLightsMap();

    void log(const char *msg, const char *type, ...) const;

    const int *getCiphersuites() const override;

private:

    QJsonDocument getAllBridgeInfos();

    void setBridgeConfig(const QJsonDocument &doc);

    void setLightsMap(const QJsonDocument &doc);

    void setGroupMap(const QJsonDocument &doc);

    //Philips Hue Bridge details
    QString _deviceModel;
    QString _deviceFirmwareVersion;
    QString _deviceAPIVersion;

    uint _api_major;
    uint _api_minor;
    uint _api_patch;
    QString _hueApplicationId;

    bool _isHueEntertainmentReady;

    QMap<QString, QJsonObject> _lightsMap;
    QMap<QString, QJsonObject> _lightStateMap;
    QMap<QString, QJsonObject> _groupsMap;

    httpResponse getRaw(const QString &route);

    void setApplicationId();
};

/**
 * Implementation for the Philips Hue system.
 *
 * To use set the device to "philipshue".
 * Uses the official Philips Hue API (http://developers.meethue.com).
 *
 * @author ntim (github), bimsarck (github)
 */
class LedDevicePhilipsHueV2 : public LedDevicePhilipsHueBridgeV2 {
Q_OBJECT


public:
    ///
    /// @brief Constructs LED-device for Philips Hue Lights system
    ///
    /// @param deviceConfig Device's configuration as JSON-Object
    ///
    LedDevicePhilipsHueV2(const QJsonObject &deviceConfig);

    ///
    /// @brief Destructor of the LED-device
    ///
    ~LedDevicePhilipsHueV2() override;

    ///
    /// @brief Constructs the LED-device
    ///
    /// @param[in] deviceConfig Device's configuration as JSON-Object
    /// @return LedDevice constructed
    static LedDevice *construct(const QJsonObject &deviceConfig);

    ///
    /// @brief Discover devices of this type available (for configuration).
    /// @note Mainly used for network devices. Allows to find devices, e.g. via ssdp, mDNS or cloud ways.
    ///
    /// @param[in] params Parameters used to overwrite discovery default behaviour
    ///
    /// @return A JSON structure holding a list of devices found
    ///
    QJsonObject discover(const QJsonObject &params) override;

    ///
    /// @brief Get the Hue Bridge device's resource properties
    ///
    /// Following parameters are required
    /// @code
    /// {
    ///     "host"  : "hostname or IP [:port]",
    ///     "user"  : "username",
    ///     "filter": "resource to query", root "/" is used, if empty
    /// }
    ///@endcode
    ///
    /// @param[in] params Parameters to query device
    /// @return A JSON structure holding the device's properties
    ///
    QJsonObject getProperties(const QJsonObject &params) override;

    ///
    /// @brief Send an update to the device to identify it.
    ///
    /// Used in context of a set of devices of the same type.
    ///
    /// @param[in] params Parameters to address device
    ///
    void identify(const QJsonObject &params) override;

    ///
    /// @brief Get the number of LEDs supported by the device.
    ///
    /// @return Number of device's LEDs
    ///
    unsigned int getLightsCount() const { return _lightsCount; }

    void setOnOffState(PhilipsHueChannel &light, bool on, bool force = false);

    void setTransitionTime(PhilipsHueChannel &light);

    void setColor(PhilipsHueChannel &light, CiColorV2 &color);

    void setState(PhilipsHueChannel &light, bool on, const CiColorV2 &color);

public slots:

    ///
    /// @brief Stops the device.
    ///
    /// Includes switching-off the device and stopping refreshes.
    ///
    void stop() override;

protected:

    ///
    /// Initialise the device's configuration
    ///
    /// @param deviceConfig Device's configuration in JSON
    /// @return True, if success
    ///
    bool init(const QJsonObject &deviceConfig) override;

    ///
    /// @brief Opens the output device
    ///
    /// @return Zero on success (i.e. device is ready), else negative
    ///
    int open() override;

    ///
    /// @brief Closes the output device.
    ///
    /// @return Zero on success (i.e. device is closed), else negative
    ///
    int close() override;

    ///
    /// @brief Writes the RGB-Color values to the LEDs.
    ///
    /// @param[in] ledValues The RGB-color per LED
    /// @return Zero on success, else negative
    ///
    int write(const std::vector<ColorRgb> &ledValues) override;

    ///
    /// @brief Switch the LEDs on.
    ///
    /// Takes care that the device is opened and powered-on.
    /// Depending on the configuration, the device may store its current state for later restore.
    /// @see powerOn, storeState
    ///
    /// @return True if success
    ///
    bool switchOn() override;

    ///
    /// @brief Switch the LEDs off.
    ///
    /// Takes care that the LEDs and device are switched-off and device is closed.
    /// Depending on the configuration, the device may be powered-off or restored to its previous state.
    /// @see powerOff, restoreState
    ///
    /// @return True, if success
    ///
    bool switchOff() override;

    bool switchOff(bool restoreState);

    ///
    /// @brief Power-/turn on the LED-device.
    ///
    /// Powers-/Turns on the LED hardware, if supported.
    ///
    /// @return True, if success
    ///
    bool powerOn() override;

    ///
    /// @brief Power-/turn off the LED-device.
    ///
    /// Depending on the device's capability, the device is powered-/turned off or
    /// an off state is simulated by writing "Black to LED" (default).
    ///
    /// @return True, if success
    ///
    bool powerOff() override;

    ///
    /// @brief Store the device's original state.
    ///
    /// Save the device's state before hyperhdr color streaming starts allowing to restore state during switchOff().
    ///
    /// @return True if success
    ///
    bool storeState() override;

    ///
    /// @brief Restore the device's original state.
    ///
    /// Restore the device's state as before hyperhdr color streaming started.
    /// This includes the on/off state of the device.
    ///
    /// @return True, if success
    ///
    bool restoreState() override;

private:

    bool initLeds();

    bool powerOn(bool wait);

    ///
    /// @brief Creates new PhilipsHueChannel(s) based on user lightid with bridge feedback
    ///
    /// @param map Map of lightid/value pairs of bridge
    ///


    bool setLights();

    bool openStream();

    bool getStreamGroupState();

    bool setStreamGroupState(bool state);

    bool startStream();

    bool stopStream();

    void writeStream(bool flush = false);

    int writeSingleLights(const std::vector<ColorRgb> &ledValues);

    ///
    bool _switchOffOnBlack;
    /// The brightness factor to multiply on color change.
    double _brightnessFactor;
    /// Transition time in multiples of 100 ms.
    /// The default of the Hue lights is 400 ms, but we may want it snappier.
    int _transitionTime;

    bool _isInitLeds;

    /// Array to save the lamps.
    std::vector<PhilipsHueChannel> _channels;

    unsigned int _lightsCount;
    QString _entertainmentConfigurationId;

    int _blackLightsTimeout;
    double _blackLevel;
    bool _candyGamma;

    uint32_t _handshake_timeout_min;
    uint32_t _handshake_timeout_max;


    bool _stopConnection;

    QString _groupName;
    QString _streamOwner;

    uint64_t _lastConfirm;
    int _lastId;
    bool _groupStreamState;
    QJsonObject _configBackup;
};
