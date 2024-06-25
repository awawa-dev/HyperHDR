#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>

	#include <vector>
#endif

#include <image/ColorRgb.h>
#include <utils/settings.h>
#include <utils/Components.h>

#define SMOOTHING_USER_CONFIG			0
#define SMOOTHING_EFFECT_CONFIGS_START	1

class Logger;
class HyperHdrInstance;

class Smoothing : public QObject
{
	Q_OBJECT

public:
	Smoothing(const QJsonDocument& config, HyperHdrInstance* hyperhdr);

	void SetEnable(bool enable);
	bool isPaused() const;
	bool isEnabled() const;

	void UpdateLedValues(const std::vector<ColorRgb>& ledValues);
	unsigned AddEffectConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool pause);
	void UpdateCurrentConfig(int settlingTime_ms);
	bool SelectConfig(unsigned cfg, bool force);
	int GetSuggestedInterval();

signals:
	void SignalMasterClockTick();	

public slots:
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);	

private slots:

	void updateLeds();
	void componentStateChange(hyperhdr::Components component, bool state);

private:

	void queueColors(const std::vector<ColorRgb>& ledColors);
	void clearQueuedColors(bool deviceEnabled = false, bool restarting = false);
	inline uint8_t computeColor(int64_t k, int color);
	void setupAdvColor(int64_t deltaTime, float& kOrg, float& kMin, float& kMid, float& kAbove, float& kMax);
	inline uint8_t computeAdvColor(int limitMin, int limitAverage, int limitMax, float kMin, float kMid, float kAbove, float kMax, int color);

	unsigned addConfig(int settlingTime_ms, double ledUpdateFrequency_hz = 25.0, bool directMode = false);
	uint8_t clamp(int x);
	void antiflickering(std::vector<ColorRgb>& newTarget);
	void linearSetup(const std::vector<ColorRgb>& ledValues);
	void linearSmoothingProcessing(bool correction);
	void debugOutput(std::vector<ColorRgb>& _targetValues);

	Logger* _log;
	HyperHdrInstance* _hyperhdr;
	int64_t _updateInterval;
	int64_t _settlingTime;
	QMutex _setupSynchro, _dataSynchro;

	std::vector<ColorRgb> _targetColorsUnsafe;
	std::vector<ColorRgb> _targetColorsCopy;
	std::vector<ColorRgb> _currentColors;
	std::vector<int64_t>  _currentTimeouts;


	bool _continuousOutput;
	int32_t _antiFlickeringTreshold;
	int32_t _antiFlickeringStep;
	int64_t _antiFlickeringTimeout;
	bool _flushFrame;
	int64_t _targetTimeUnsafe;
	int64_t _targetTimeCopy;
	int64_t _previousTime;
	bool _pause;

	enum class SmoothingType { Linear = 0, Alternative = 1 };

	struct SmoothingConfig
	{
		bool		  pause;
		int64_t		  settlingTime;
		int64_t		  updateInterval;
		SmoothingType type;
		int			  antiFlickeringTreshold;
		int			  antiFlickeringStep;
		int64_t		  antiFlickeringTimeout;

        SmoothingConfig(bool __pause, int64_t __settlingTime, int64_t __updateInterval,
            SmoothingType __type = SmoothingType::Linear, int __antiFlickeringTreshold = 0, int __antiFlickeringStep = 0,
            int64_t __antiFlickeringTimeout = 0);

		static QString EnumToString(SmoothingType type);
	};

	std::vector<std::unique_ptr<SmoothingConfig>> _configurations;

	unsigned		_currentConfigId;
	std::atomic_bool _enabled;
	std::atomic_bool _hasData;
	bool			_connected;
	SmoothingType	_smoothingType;
	bool			_infoUpdate;
	bool			_infoInput;
	int				_coolDown;
};
