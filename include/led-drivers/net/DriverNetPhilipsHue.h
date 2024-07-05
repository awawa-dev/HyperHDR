#pragma once

#ifndef PCH_ENABLED
	#include <QStringList>
	#include <QNetworkAccessManager>
	#include <QNetworkReply>

	#include <set>
	#include <string>
	#include <stdarg.h>
#endif

#include <led-drivers/LedDevice.h>
#include "ProviderRestApi.h"
#include "ProviderUdpSSL.h"

struct XYColor
{
	double x;
	double y;
};

struct CiColorTriangle
{
	XYColor red, green, blue;
};

struct CiColor
{
	double x;
	double y;
	double bri;

	/// Converts an RGB color to the Hue xy color space and brightness.
	/// https://github.com/PhilipsHue/PhilipsHueSDK-iOS-OSX/blob/master/ApplicationDesignNotes/RGB%20to%20xy%20Color%20conversion.md
	static CiColor rgbToCiColor(double red, double green, double blue, const CiColorTriangle& colorSpace, bool candyGamma);

	static bool isPointInLampsReach(CiColor p, const CiColorTriangle& colorSpace);
	static double crossProduct(XYColor p1, XYColor p2);
	static XYColor getClosestPointToPoint(XYColor a, XYColor b, CiColor p);
	static double getDistanceBetweenTwoPoints(CiColor p1, XYColor p2);
};

bool operator==(const CiColor& p1, const CiColor& p2);
bool operator!=(const CiColor& p1, const CiColor& p2);

class PhilipsHueLight
{

public:
	// Hue system model ids (http://www.developers.meethue.com/documentation/supported-lights).
	static const std::set<QString> GAMUT_A_MODEL_IDS;
	static const std::set<QString> GAMUT_B_MODEL_IDS;
	static const std::set<QString> GAMUT_C_MODEL_IDS;

	PhilipsHueLight(Logger* log, unsigned int id, QJsonObject values, unsigned int ledidx,
		int onBlackTimeToPowerOff,
		int onBlackTimeToPowerOn);
	PhilipsHueLight(Logger* log, unsigned int id, QJsonObject values, QStringList lightIds,
		unsigned int ledidx);
	~PhilipsHueLight();

	void setOnOffState(bool on);
	void setTransitionTime(int transitionTime);
	void setColor(const CiColor& color);

	unsigned int getId() const;

	bool getOnOffState() const;
	int getTransitionTime() const;
	CiColor getColor() const;
	bool hasColor();

	CiColorTriangle getColorSpace() const;

	void saveOriginalState(const QJsonObject& values);
	QString getOriginalState() const;
	bool isBusy();
	bool isBlack(bool isBlack);
	bool isWhite(bool isWhite);
	void setBlack();
	void blackScreenTriggered();	
	ColorRgb getRGBColor() const;
	void setRGBColor(const ColorRgb color);
	QStringList getLightIds() const;

private:

	Logger* _log;
	unsigned int _id;
	unsigned int _ledidx;
	bool _on;
	int _transitionTime;
	CiColor _color;
	ColorRgb _colorRgb;
	bool    _hasColor;
	CiColor _colorBlack;
	QString _modelId;
	QString _lightname;
	CiColorTriangle _colorSpace;

	QJsonObject _originalStateJSON;

	QString _originalState;
	CiColor _originalColor;
	uint64_t _lastSendColor;
	uint64_t _lastBlack;
	uint64_t _lastWhite;
	bool _blackScreenTriggered;
	uint64_t _onBlackTimeToPowerOff;
	uint64_t _onBlackTimeToPowerOn;
	QStringList _lightIds;
};

class LedDevicePhilipsHueBridge : public ProviderUdpSSL
{
	Q_OBJECT

public:

	explicit LedDevicePhilipsHueBridge(const QJsonObject& deviceConfig);
	~LedDevicePhilipsHueBridge() override;

	bool initRestAPI(const QString& hostname, int port, const QString& token);
	QJsonDocument get(const QString& route);
	QJsonDocument post(const QString& route, const QString& content, bool supressError = false);

	QJsonDocument getLightState(unsigned int lightId);
	void setLightState(unsigned int lightId = 0, const QString& state = "");

	QMap<quint16, QJsonObject> getLightMap() const;

	QMap<quint16, QJsonObject> getGroupMap() const;

	QString getGroupName(quint16 groupId = 0) const;

	QJsonArray getGroupLights(quint16 groupId = 0) const;

	QJsonDocument getLightStateV2(QString lightId);
	void setLightStateV2(QString lightId = "", const QString& state = "");
	QMap<QString, QJsonObject> getGroupMapV2() const;
	QString getGroupNameV2(QString groupId = "0") const;
	QJsonArray getGroupChannelsV2(QString groupId = 0) const;
	void setApplicationIdV2();
	bool isApiV2();
	void setApiV2(bool version2);

protected:

	bool init(const QJsonObject& deviceConfig) override;
	bool checkApiError(const QJsonDocument& response, bool supressError = false);

	ProviderRestApi* _restApi;
	QString _hostname;
	int _apiPort;	
	QString _username;
	bool	_useHueEntertainmentAPI;

	QJsonDocument getGroupState(unsigned int groupId);
	QJsonDocument setGroupState(unsigned int groupId, bool state);

	bool isStreamOwner(const QString& streamOwner) const;
	bool initMaps();

	void log(const char* msg, const char* type, ...) const;

	std::list<QString> getCiphersuites() override;

	QJsonDocument getGroupStateV2(QString groupId);
	QJsonDocument setGroupStateV2(QString groupId, bool state);
	QStringList getLightIdsInChannelV2(QJsonObject channel);
	QMap<QString, QJsonObject>& getLightStateMapV2();
	void setLightsMapV2(const QJsonDocument& doc);
	void setGroupMapV2(const QJsonDocument& doc);
	httpResponse getRaw(const QString& route);

private:

	QJsonDocument getAllBridgeInfos();
	void setBridgeConfig(const QJsonDocument& doc);
	void setLightsMap(const QJsonDocument& doc);
	void setGroupMap(const QJsonDocument& doc);

	QString _deviceModel;
	QString _deviceFirmwareVersion;
	QString _deviceAPIVersion;

	uint _api_major;
	uint _api_minor;
	uint _api_patch;

	bool _isHueEntertainmentReady;

	QMap<quint16, QJsonObject> _lightsMap;
	QMap<quint16, QJsonObject> _groupsMap;

	QString _hueApplicationId;
	QMap<QString, QJsonObject> _lightsMapV2;
	QMap<QString, QJsonObject> _lightStateMapV2;
	QMap<QString, QJsonObject> _groupsMapV2;

	bool _apiV2;
};

class DriverNetPhilipsHue : public LedDevicePhilipsHueBridge
{
	Q_OBJECT

public:
	explicit DriverNetPhilipsHue(const QJsonObject& deviceConfig);
	~DriverNetPhilipsHue() override;
	static LedDevice* construct(const QJsonObject& deviceConfig);
	QJsonObject discover(const QJsonObject& params) override;
	QJsonObject getProperties(const QJsonObject& params) override;
	void identify(const QJsonObject& params) override;
	unsigned int getLightsCount() const { return _lightsCount; }

	void setOnOffState(PhilipsHueLight& light, bool on, bool force = false);
	void setTransitionTime(PhilipsHueLight& light);
	void setColor(PhilipsHueLight& light, CiColor& color);
	void setState(PhilipsHueLight& light, bool on, const CiColor& color);

public slots:
	void stop() override;

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int close() override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	bool switchOn() override;
	bool switchOff() override;
	bool powerOn() override;
	bool powerOff() override;
	bool storeState() override;
	bool restoreState() override;
	void colorChannel(const ColorRgb& colorRgb, unsigned int i);

private:
	bool initLeds(QString groupName);
	bool powerOn(bool wait);
	bool setLights();
	bool updateLights(const QMap<quint16, QJsonObject>& map);
	void setLightsCount(unsigned int lightsCount);

	bool openStream();
	bool getStreamGroupState();
	bool setStreamGroupState(bool state);
	bool startStream();
	bool stopStream();

	void writeStream(bool flush = false);
	int writeSingleLights(const std::vector<ColorRgb>& ledValues);

	std::vector<uint8_t> prepareStreamData() const;
	std::vector<uint8_t> prepareStreamDataV2();

	bool _switchOffOnBlack;
	double _brightnessFactor;
	int _transitionTime;
	bool _isInitLeds;

	std::vector<quint16> _lightIds;
	std::vector<PhilipsHueLight> _lights;

	unsigned int _lightsCount;
	QString		_entertainmentConfigurationId;
	quint16		_groupId;

	int			_blackLightsTimeout;
	double		_blackLevel;
	int			_onBlackTimeToPowerOff;
	int			_onBlackTimeToPowerOn;
	bool		_candyGamma;

	uint32_t    _handshake_timeout_min;
	uint32_t    _handshake_timeout_max;

	bool		_stopConnection;

	QString		_groupName;
	QString		_streamOwner;

	uint64_t	_lastConfirm;
	int			_lastId;
	bool		_groupStreamState;
	QJsonObject _configBackup;
	std::atomic<uint8_t> _sequenceNumber;

	static bool isRegistered;
};
