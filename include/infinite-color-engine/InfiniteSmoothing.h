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
#include <utils/Logger.h>

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
	void queueColors(SharedOutputColors&& nonlinearRgbLedColors);
	void clearQueuedColors(bool deviceEnabled = false, bool restarting = false);

	unsigned addConfig(int settlingTime_ms, double ledUpdateFrequency_hz = 25.0, bool pause = false);

	LoggerName _log;
	HyperHdrInstance* _hyperhdr;
	QMutex _dataSynchro;

	bool _continuousOutput;

	enum class SmoothingType { Stepper = 0, RgbInterpolator = 1, YuvInterpolator = 2, HybridInterpolator = 3, ExponentialInterpolator = 4, HybridRgbInterpolator = 5};
	static QString EnumSmoothingTypeToString(SmoothingType type);
	static SmoothingType StringToEnumSmoothingType(QString name);

	struct SmoothingConfig
	{
		bool		  pause;
		int64_t		  settlingTime;
		int64_t		  updateInterval;
		SmoothingType type;
		float		  smoothingFactor;
		float		  stiffness;
		float		  damping;
		float		  y_limit;
	};

	std::vector<std::unique_ptr<SmoothingConfig>> _configurations;

	unsigned		_currentConfigId;
	std::atomic_bool _enabled;
	bool			_connected;
	std::unique_ptr<InfiniteInterpolator> _interpolator;
	bool			_infoUpdate;
	bool			_infoInput;
	int				_coolDown;
	long long		_lastSentFrame;
};
