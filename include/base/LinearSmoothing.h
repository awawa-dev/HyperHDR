#pragma once

// STL includes
#include <vector>

// Qt includes
#include <QVector>

// hyperhdr incluse
#include <leddevice/LedDevice.h>
#include <utils/Components.h>

// settings
#include <utils/settings.h>

class QTimer;
class Logger;
class HyperHdrInstance;

/// Linear Smooting class
///
/// This class processes the requested led values and forwards them to the device after applying
/// a linear smoothing effect. This class can be handled as a generic LedDevice.
class LinearSmoothing : public QObject
{
	Q_OBJECT

public:
	/// Constructor
	/// @param config    The configuration document smoothing
	/// @param hyperhdr  The HyperHDR parent instance
	///
	LinearSmoothing(const QJsonDocument& config, HyperHdrInstance* hyperhdr);
	~LinearSmoothing();

	/// LED values as input for the smoothing filter
	///
	/// @param ledValues The color-value per led
	/// @return Zero on success else negative
	///	

	void setEnable(bool enable);
	bool pause() const;
	bool enabled() const;		

public slots:
	///
	/// @brief Handle settings update from HyperHDR Settingsmanager emit or this constructor
	/// @param type   settingyType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

	///
	/// @brief select a smoothing cfg given by cfg index from addConfig()
	/// @param   cfg     The index to use
	/// @param   force   Overwrite in any case the current values (used for cfg 0 settings update)
	///
	/// @return  On success return else false (and falls back to cfg 0)
	///
	bool selectConfig(unsigned cfg, bool force); // = false

	/// Timer callback which writes updated led values to the led device
	void updateLeds();

	///
	/// @brief Handle component state changes
	/// @param component   The component
	/// @param state       The requested state
	///
	void componentStateChange(hyperhdr::Components component, bool state);

	void updateLedValues(std::vector<ColorRgb> ledValues);

	///
	/// @brief Update a smoothing cfg which can be used with selectConfig()
	///	       In case the ID does not exist, a smoothing cfg is added
	///
	/// @param   cfgID				   Smoothing configuration item to be updated
	/// @param   settlingTime_ms       The buffer time
	/// @param   ledUpdateFrequency_hz The frequency of update
	/// @param   updateDelay           The delay
	///
	/// @return The index of the cfg which can be passed to selectConfig()
	///
	unsigned updateConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool directModee);
private:

	/**
	 * Pushes the colors into the output queue and popping the head to the led-device
	 *
	 * @param ledColors The colors to queue
	 */
	void queueColors(const std::vector<ColorRgb>& ledColors);
	void clearQueuedColors(bool deviceEnabled = false, bool restarting = false);
	inline uint8_t computeColor(int64_t k, int color);
	void setupAdvColor(int64_t deltaTime, float& kOrg, float& kMin, float& kMid, float& kAbove, float& kMax);
	inline uint8_t computeAdvColor(int limitMin, int limitAverage, int limitMax, float kMin, float kMid, float kAbove, float kMax, int color);

	///
	/// @brief Add a new smoothing cfg which can be used with selectConfig()
	/// @param   settlingTime_ms       The buffer time
	/// @param   ledUpdateFrequency_hz The frequency of update
	/// @param   updateDelay           The delay
	///
	/// @return The index of the cfg which can be passed to selectConfig()
	///
	unsigned addConfig(int settlingTime_ms, double ledUpdateFrequency_hz = 25.0, bool directMode = false);

	uint8_t clamp(int x);

	void Antiflickering();

	void LinearSetup(const std::vector<ColorRgb>& ledValues);

	void LinearSmoothingProcessing(bool correction);

	void DebugOutput();

	/// Logger instance
	Logger* _log;

	/// HyperHDR instance
	HyperHdrInstance* _hyperhdr;

	/// The interval at which to update the leds (msec)
	int64_t _updateInterval;

	/// The time after which the updated led values have been fully applied (msec)
	int64_t _settlingTime;

	/// The Qt timer object
	QTimer* _timer;

	/// The target led data
	std::vector<ColorRgb> _targetValues;

	/// The previously written led data
	std::vector<ColorRgb> _previousValues;
	std::vector<int64_t>  _previousTimeouts;

	/// Flag for dis/enable continuous output to led device regardless there is new data or not
	bool _continuousOutput;

	int32_t _antiFlickeringTreshold;

	int32_t _antiFlickeringStep;

	int64_t _antiFlickeringTimeout;

	bool _flushFrame;

	int64_t _targetTime;

	int64_t _previousTime;

	/// Flag for pausing
	bool _pause;

	enum class SmoothingType { Linear = 0, Alternative = 1 };

	class SmoothingCfg
	{
	public:
		bool		  _pause;
		int64_t		  _settlingTime;
		int64_t		  _updateInterval;
		bool		  _directMode;
		SmoothingType _type;
		int			  _antiFlickeringTreshold;
		int			  _antiFlickeringStep;
		int64_t		  _antiFlickeringTimeout;

		SmoothingCfg();

		SmoothingCfg(bool pause, int64_t settlingTime, int64_t updateInterval, bool directMode, SmoothingType type = SmoothingType::Linear, int antiFlickeringTreshold = 0, int antiFlickeringStep = 0, int64_t antiFlickeringTimeout = 0);

		static QString EnumToString(SmoothingType type);
	};

	/// smooth config list
	QVector<SmoothingCfg> _cfgList;

	unsigned	  _currentConfigId;
	bool		  _enabled;
	bool		  _directMode;
	SmoothingType _smoothingType;
	bool		  _infoUpdate;
	bool		  _infoInput;
	int			  debugCounter;
};
