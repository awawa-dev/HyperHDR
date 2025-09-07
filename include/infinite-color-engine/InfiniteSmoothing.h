#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>
	#include <QObject>
	#include <QTimer>

	#include <vector>
	#include <memory>
	#include <atomic>
	#include <cmath>
	#include <cstdint>
	#include <inttypes.h>
#endif

#include <image/ColorRgb.h>
#include <utils/settings.h>
#include <utils/Components.h>
#include <utils/InternalClock.h>
#include <infinite-color-engine/SharedOutputColors.h>
#include <infinite-color-engine/InfiniteInterpolator.h>

class Logger;
class HyperHdrInstance;

class InfiniteSmoothing : public QObject
{
	Q_OBJECT

public:
	InfiniteSmoothing(const QJsonDocument& config, HyperHdrInstance* hyperhdr);

	void setEnable(bool enable);
	bool isEnabled() const;

	void incomingColors(std::vector<linalg::aliases::float3>&& nonlinearRgbColors);
	unsigned addCustomSmoothingConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool pause);
	void setCurrentSmoothingConfigParams(unsigned cfgID);
	bool selectConfig(unsigned cfgId);
	int getSuggestedInterval();

	static constexpr auto SMOOTHING_EFFECT_CONFIGS_START = 1;

signals:
	void SignalMasterClockTick();
	void SignalProcessedColors(SharedOutputColors nonlinearRgbColors);

public slots:
	void handleSignalInstanceSettingsChanged(settings::type type, const QJsonDocument& config);
	void handleSignalRequestComponent(hyperhdr::Components component, bool state);

private slots:
	void updateLeds();
	
private:
	void queueColors(SharedOutputColors&& ledColors);
	void clearQueuedColors(bool deviceEnabled = false, bool restarting = false);

	unsigned addConfig(int settlingTime_ms, double ledUpdateFrequency_hz = 25.0, bool directMode = false);

	Logger* _log;
	HyperHdrInstance* _hyperhdr;
	QMutex _dataSynchro;

	bool _continuousOutput;
	bool _flushFrame;

	enum class SmoothingType { Stepper = 0, RgbInterpolator = 1, YuvInterpolator = 2, HybridInterpolator = 3};
	static QString EnumSmoothingTypeToString(SmoothingType type);
	static SmoothingType StringToEnumSmoothingType(QString name);

	struct SmoothingConfig
	{
		bool		  pause;
		int64_t		  settlingTime;
		int64_t		  updateInterval;
		SmoothingType type;
	};

	std::vector<std::unique_ptr<SmoothingConfig>> _configurations;

	unsigned		_currentConfigId;
	std::atomic_bool _enabled;
	bool			_connected;
	SmoothingType	_smoothingType;
	std::unique_ptr<InfiniteInterpolator> _interpolator;
	bool			_infoUpdate;
	bool			_infoInput;
	int				_coolDown;
};
