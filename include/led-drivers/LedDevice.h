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
	#include <utility>	
#endif

#include <led-drivers/LedDeviceManufactory.h>
#include <image/MemoryBuffer.h>
#include <image/ColorRgb.h>
#include <utils/Logger.h>
#include <utils/Components.h>
#include <led-drivers/ColorRgbw.h>
#include <performance-counters/PerformanceCounters.h>
#include <infinite-color-engine/SharedOutputColors.h>

class LedDevice;
class DiscoveryWrapper;

typedef LedDevice* (*LedDeviceCreateFuncType) (const QJsonObject&);
typedef std::map<QString, LedDeviceCreateFuncType> LedDeviceRegistry;

class LedDevice : public QObject
{
	Q_OBJECT

public:

	LedDevice(const QJsonObject& deviceConfig = QJsonObject(), QObject* parent = nullptr);
	virtual ~LedDevice() = default;

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
	void handleSignalFinalOutputColorsReady(SharedOutputColors nonlinearRgbColors);
	int getRefreshTime() const;
	int getLedCount() const;
	QString getActiveDeviceType() const;
	bool componentState() const;
	void enable();
	void disable();
	virtual bool switchOn();
	virtual bool switchOff();
	void blinking(QJsonObject params);
	void smoothingRestarted(int newSmoothingInterval, bool antiflickeringfilter);
	int hasLedClock();
	void pauseRetryTimer(bool mode);

signals:
	void SignalEnableStateChanged(bool newState);
	void SignalManualUpdate();
	void SignalSmoothingClockTick();

protected:

	virtual bool init(QJsonObject deviceConfig);
	virtual int open();
	virtual int close();
	int write(SharedOutputColors nonlinearRgbColors);
	virtual std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors);
	virtual int writeFiniteColors(const std::vector<ColorRgb>& ledValues);
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
	LoggerName _log;

	std::vector<uint8_t> _ledBuffer;
	QTimer* _refreshTimer;

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

	bool	_antiFlickeringFilter;
	int		_maxRetry;
	int		_currentRetry;
	QString _customInfo;
	QTimer* _retryTimer;

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
	SharedOutputColors	_lastLedValues;
	SharedOutputColors	_lastFinityLedValues;

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
