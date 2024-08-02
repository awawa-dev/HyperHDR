#pragma once

#ifndef PCH_ENABLED	
	#include <QHostAddress>
	#include <chrono>
#endif

#include <led-drivers/LedDevice.h>

class QTcpSocket;
class QTcpServer;

namespace {
	const char API_METHOD_POWER[] = "set_power";
	const char API_METHOD_POWER_ON[] = "on";
	const char API_METHOD_POWER_OFF[] = "off";

	const char API_METHOD_MUSIC_MODE[] = "set_music";
	const int API_METHOD_MUSIC_MODE_ON = 1;
	const int API_METHOD_MUSIC_MODE_OFF = 0;

	const char API_METHOD_SETRGB[] = "set_rgb";
	const char API_METHOD_SETSCENE[] = "set_scene";
	const char API_METHOD_GETPROP[] = "get_prop";

	const char API_PARAM_EFFECT_SUDDEN[] = "sudden";
	const char API_PARAM_EFFECT_SMOOTH[] = "smooth";

	constexpr std::chrono::milliseconds API_PARAM_DURATION{ 50 };
	constexpr std::chrono::milliseconds API_PARAM_DURATION_POWERONOFF{ 1000 };
	constexpr std::chrono::milliseconds API_PARAM_EXTRA_TIME_DARKNESS{ 200 };

}

class YeelightResponse
{
public:

	enum API_REPLY {
		API_OK,
		API_ERROR,
		API_NOTIFICATION,
	};

	API_REPLY error() const { return _error; }
	void setError(const YeelightResponse::API_REPLY replyType) { _error = replyType; }

	QJsonArray getResult() const { return _resultArray; }
	void setResult(const QJsonArray& result) { _resultArray = result; }

	int getErrorCode() const { return _errorCode; }
	void setErrorCode(int errorCode) { _errorCode = errorCode; _error = API_ERROR; }

	QString getErrorReason() const { return _errorReason; }
	void setErrorReason(const QString& errorReason) { _errorReason = errorReason; }

private:

	QJsonArray _resultArray;
	API_REPLY _error = API_OK;

	int _errorCode = 0;
	QString _errorReason;
};

class YeelightLight
{

public:
	enum API_EFFECT {
		API_EFFECT_SMOOTH,
		API_EFFECT_SUDDEN
	};

	enum API_MODE {
		API_TURN_ON_MODE,
		API_CT_MODE,
		API_RGB_MODE,
		API_HSV_MODE,
		API_COLOR_FLOW_MODE,
		API_NIGHT_LIGHT_MODE
	};

	YeelightLight(Logger* log, const QString& hostname, quint16 port);
	virtual ~YeelightLight();

	void setHostname(const QString& hostname, quint16 port);
	void setName(const QString& name) { _name = name; }
	QString getName() const { return _name; }
	bool open();
	bool close();
	bool identify();
	int writeCommand(const QJsonDocument& command);
	int writeCommand(const QJsonDocument& command, QJsonArray& result);
	bool streamCommand(const QJsonDocument& command);
	void setStreamSocket(QTcpSocket* socket);
	bool setPower(bool on);
	bool setPower(bool on, API_EFFECT effect, int duration, API_MODE mode = API_RGB_MODE);
	bool setColorRGB(const ColorRgb& color);
	bool setColorHSV(const ColorRgb& color);
	void setTransitionEffect(API_EFFECT effect, int duration = API_PARAM_DURATION.count());
	void setBrightnessConfig(int min = 1, int max = 100, bool switchoff = false, int extraTime = 0, double factor = 1);
	bool setMusicMode(bool on, const QHostAddress& hostAddress = {}, int port = -1);
	void setQuotaWaitTime(int waitTime) { _waitTimeQuota = waitTime; }
	QJsonObject getProperties();
	void storeState();
	bool restoreState();
	bool wasOriginallyOn() const { return _power == API_METHOD_POWER_ON ? true : false; }
	bool isReady() const { return !_isInError; }
	bool isOn() const { return _isOn; }
	bool isInMusicMode(bool deviceCheck = false);
	void setInError(const QString& errorMsg);
	void setDebuglevel(int level) { _debugLevel = level; }

private:

	YeelightResponse handleResponse(int correlationID, QByteArray const& response);

	QJsonDocument getCommand(const QString& method, const QJsonArray& params);
	void mapProperties(const QJsonObject& properties);
	void log(int logLevel, const char* msg, const char* type, ...);

	Logger* _log;
	int _debugLevel;
	bool _isInError;
	QString _host;
	quint16 _port;
	QTcpSocket* _tcpSocket;
	QTcpSocket* _tcpStreamSocket;
	int _correlationID;
	qint64	_lastWriteTime;
	ColorRgb _color;
	int _lastColorRgbValue;

	API_EFFECT _transitionEffect;
	int _transitionDuration;
	int _extraTimeDarkness;

	int _brightnessMin;
	bool _isBrightnessSwitchOffMinimum;
	int _brightnessMax;
	double _brightnessFactor;

	QString _transitionEffectParam;
	int _waitTimeQuota;
	QJsonObject _originalStateProperties;
	QString _name;
	QString _model;
	QString _power;
	QString _fw_ver;
	int _colorRgbValue;
	int _bright;
	int _ct;
	bool _isOn;
	bool _isInMusicMode;
};

class DriverNetYeelight : public LedDevice
{
public:
	explicit DriverNetYeelight(const QJsonObject& deviceConfig);
	~DriverNetYeelight() override;

	static LedDevice* construct(const QJsonObject& deviceConfig);
	QJsonObject discover(const QJsonObject& params) override;
	QJsonObject getProperties(const QJsonObject& params) override;
	void identify(const QJsonObject& params) override;

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int close() override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	bool powerOn() override;
	bool powerOff() override;
	bool storeState() override;
	bool restoreState() override;

private:

	struct yeelightAddress {
		QString host;
		int port;

		bool operator == (yeelightAddress const& a) const
		{
			return ((host == a.host) && (port == a.port));
		}
	};

	enum COLOR_MODEL {
		MODEL_HSV,
		MODEL_RGB
	};

	bool startMusicModeServer();
	bool stopMusicModeServer();
	bool updateLights(const QVector<yeelightAddress>& list);
	void setLightsCount(unsigned int lightsCount) { _lightsCount = lightsCount; }
	uint getLightsCount() const { return _lightsCount; }
	QVector<yeelightAddress> _lightsAddressList;
	std::vector<YeelightLight> _lights;
	unsigned int _lightsCount;
	int _outputColorModel;
	YeelightLight::API_EFFECT _transitionEffect;
	int _transitionDuration;
	int _extraTimeDarkness;

	int _brightnessMin;
	bool _isBrightnessSwitchOffMinimum;
	int _brightnessMax;
	double _brightnessFactor;

	int _waitTimeQuota;

	int _debuglevel;

	QHostAddress _musicModeServerAddress;
	int _musicModeServerPort;
	QTcpServer* _tcpMusicModeServer = nullptr;

	static bool isRegistered;
};
