#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <QJsonObject>
	#include <QJsonArray>
	#include <QJsonDocument>
	#include <QTimer>
	#include <QDateTime>

	#include <vector>
	#include <map>
	#include <algorithm>
	#include <functional>
	#include <atomic>
#endif

#include <led-drivers/LedDeviceManufactory.h>
#include <image/MemoryBuffer.h>
#include <image/ColorRgb.h>
#include <utils/Logger.h>
#include <utils/Components.h>
#include <led-drivers/ColorRgbw.h>
#include <performance-counters/PerformanceCounters.h>

class LedDevice;
class DiscoveryWrapper;

typedef LedDevice* (*LedDeviceCreateFuncType) (const QJsonObject&);
typedef std::map<QString, LedDeviceCreateFuncType> LedDeviceRegistry;

class LedDevice : public QObject
{
	Q_OBJECT

public:

	LedDevice(const QJsonObject& deviceConfig = QJsonObject(), QObject* parent = nullptr);
	virtual ~LedDevice();

	void setActiveDeviceType(const QString& deviceType);
	void setLedCount(int ledCount);
	void setRefreshTime(int userInterval);
	virtual QJsonObject discover(const QJsonObject& params);
	virtual QString discoverFirst();
	virtual QJsonObject getProperties(const QJsonObject& params);
	virtual void identify(const QJsonObject& /*params*/) {}
	virtual void identifyLed(const QJsonObject& /*params*/);
	bool isInitialised() const;
	bool isReady() const;
	bool isInError() const;
	void setInstanceIndex(int instanceIndex);

	static void printLedValues(const std::vector<ColorRgb>& ledValues);
	static void signalTerminateTriggered();

public slots:
	virtual void start();
	virtual void stop();
	void updateLeds(std::vector<ColorRgb> ledValues);
	int getRefreshTime() const;
	int getLedCount() const;
	QString getActiveDeviceType() const;
	bool componentState() const;
	void enable();
	void disable();
	virtual bool switchOn();
	virtual bool switchOff();
	void blinking(QJsonObject params);
	void smoothingRestarted(int newSmoothingInterval);
	int hasLedClock();
	void pauseRetryTimer(bool mode);

signals:
	void SignalEnableStateChanged(bool newState);
	void SignalManualUpdate();
	void SignalSmoothingClockTick();

protected:

	virtual bool init(const QJsonObject& deviceConfig);
	virtual int open();
	virtual int close();
	virtual int write(const std::vector<ColorRgb>& ledValues) = 0;
	virtual int writeBlack(int numberOfBlack = 2);
	virtual bool powerOn();
	virtual bool powerOff();
	virtual bool storeState();
	virtual bool restoreState();

	void enableDevice(bool toEmit);
	void disableDevice(bool toEmit);
	void startRefreshTimer();
	void setupRetry(int interval);

	QString uint8_t_to_hex_string(const uint8_t* data, const int size, int number = -1) const;
	QString toHex(const QByteArray& data, int number = -1) const;

	QString _activeDeviceType;
	QJsonObject _devConfig;
	Logger* _log;

	std::vector<uint8_t> _ledBuffer;
	std::unique_ptr<QTimer> _refreshTimer;

	int _currentInterval;
	int _defaultInterval;
	int _forcedInterval;
	int _smoothingInterval;
	
	uint _ledCount;
	uint _ledRGBCount;
	uint _ledRGBWCount;

	bool	_isRestoreOrigState;
	QJsonObject _orignalStateValues;
	bool _isEnabled;
	bool _isDeviceInitialised;
	bool _isDeviceReady;
	bool _isOn;
	bool _isDeviceInError;

	int		_maxRetry;
	int		_currentRetry;
	QString _customInfo;
	std::unique_ptr<QTimer> _retryTimer;

	static std::atomic<bool> _signalTerminate;

	std::weak_ptr<DiscoveryWrapper> _discoveryWrapper;

protected slots:
	int rewriteLEDs();
	virtual void setInError(const QString& errorMsg);

private:
	void stopRefreshTimer();
	void stopRetryTimer();

	std::atomic_bool	_isRefreshEnabled;
	std::atomic_bool	_newFrame2Send;
	std::vector<ColorRgb> _lastLedValues;

	struct LedStats
	{
		qint64		token = 0;
		qint64		statBegin = 0;
		qint64		frames = 0;
		qint64		incomingframes = 0;
		qint64		droppedFrames = 0;

		void reset(int64_t now);
	} _computeStats;

	int _blinkIndex;
	qint64 _blinkTime;
	int _instanceIndex;
	int _pauseRetryTimer;
};
