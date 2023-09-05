#ifndef LEDEVICE_H
#define LEDEVICE_H

// qt includes
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QDateTime>

// STL includes
#include <vector>
#include <map>
#include <algorithm>
#include <atomic>

// Utility includes
#include <utils/ColorRgb.h>
#include <utils/ColorRgbw.h>
#include <utils/Logger.h>
#include <functional>
#include <utils/Components.h>
#include <utils/PerformanceCounters.h>

class LedDevice;

typedef LedDevice* (*LedDeviceCreateFuncType) (const QJsonObject&);
typedef std::map<QString, LedDeviceCreateFuncType> LedDeviceRegistry;

///
/// @brief Interface (pure virtual base class) for LED-devices.
///
class LedDevice : public QObject
{
	Q_OBJECT

public:

	///
	/// @brief Constructs LED-device
	///
	/// @param deviceConfig Device's configuration as JSON-Object
	/// @param parent QT parent
	///
	LedDevice(const QJsonObject& deviceConfig = QJsonObject(), QObject* parent = nullptr);

	///
	/// @brief Destructor of the LED-device
	///
	~LedDevice() override;

	///
	/// @brief Set the current active LED-device type.
	///
	/// @param deviceType Device's type
	///
	void setActiveDeviceType(const QString& deviceType);

	///
	/// @brief Set the number of LEDs supported by the device.
	///
	/// @param[in] ledCount Number of device LEDs,  0 = unknown number
	///
	void setLedCount(int ledCount);

	///
	/// @brief Set a device's refresh time.
	///
	/// Refresh time is the time frame a devices requires to be refreshed, if no updated happened in the meantime.
	///
	/// @param[in] refreshTime_ms refresh time in milliseconds
	///
	void setRefreshTime(int refreshTime_ms);

	///
	/// @brief Discover devices of this type available (for configuration).
	/// @note Mainly used for network devices. Allows to find devices, e.g. via ssdp, mDNS or cloud ways.
	///
	/// @param[in] params Parameters used to overwrite discovery default behaviour
	///
	/// @return A JSON structure holding a list of devices found
	///
	virtual QJsonObject discover(const QJsonObject& params);

	///
	/// @brief Discover first device of this type available (for configuration).
	/// @note Mainly used for network devices. Allows to find devices, e.g. via ssdp, mDNS or cloud ways.
	///
	/// @return A string of the device found
	///
	virtual QString discoverFirst();

	///
	/// @brief Get the device's properties
	///
	/// Used in context of a set of devices of the same type.
	///
	/// @param[in] params Parameters to address device
	/// @return A JSON structure holding the device's properties
	///
	virtual QJsonObject getProperties(const QJsonObject& params);

	///
	/// @brief Send an update to the device to identify it.
	///
	/// Used in context of a set of devices of the same type.
	///
	/// @param[in] params Parameters to address device
	///
	virtual void identify(const QJsonObject& /*params*/) {}

	virtual void identifyLed(const QJsonObject& /*params*/);

	///
	/// @brief Check, if device is properly initialised
	///
	/// i.e. initialisation and configuration were successful.
	///
	/// @return True, if device is initialised
	///
	bool isInitialised() const;

	///
	/// @brief Check, if device is ready to be used.
	///
	/// i.e. initialisation and opening were successful.
	///
	/// @return True, if device is ready
	///
	bool isReady() const;

	///
	/// @brief Check, if device is in error state.
	///
	/// @return True, if device is in error
	///
	bool isInError() const;

	///
	/// @brief Prints the color values to stdout.
	///
	/// @param[in] ledValues The color per led
	///
	static void printLedValues(const std::vector<ColorRgb>& ledValues);

	static void signalTerminateTriggered();

public slots:

	///
	/// @brief Is called on thread start, all construction tasks and init should run here.
	///
	virtual void start();

	///
	/// @brief Stops the device.
	///
	/// Includes switching-off the device and stopping refreshes.
	///
	virtual void stop();

	///
	/// @brief Update the color values of the device's LEDs.
	///
	/// Handles refreshing of LEDs.
	///
	/// @param[in] ledValues The color per LED
	/// @return Zero on success else negative (i.e. device is not ready)
	///
	virtual int updateLeds(std::vector<ColorRgb> ledValues);

	///
	/// @brief Get the currently defined RefreshTime.
	///
	/// @return Rewrite time in milliseconds
	///
	int getRefreshTime() const;

	///
	/// @brief Get the number of LEDs supported by the device.
	///
	/// @return Number of device's LEDs, 0 = unknown number
	///
	int getLedCount() const;

	///
	/// @brief Get the current active LED-device type.
	///
	QString getActiveDeviceType() const;

	///
	/// @brief Get the LED-Device component's state.
	///
	/// @return True, if enabled
	///
	bool componentState() const;

	///
	/// @brief Enables the device for output.
	///
	/// If the device is not ready, it will not be enabled.
	///
	void enable();

	///
	/// @brief Disables the device for output.
	///
	void disable();

	///
	/// @brief Switch the LEDs on.
	///
	/// Takes care that the device is opened and powered-on.
	/// Depending on the configuration, the device may store its current state for later restore.
	/// @see powerOn, storeState
	///
	/// @return True, if success
	///
	virtual bool switchOn();

	///
	/// @brief Switch the LEDs off.
	///
	/// Takes care that the LEDs and device are switched-off and device is closed.
	/// Depending on the configuration, the device may be powered-off or restored to its previous state.
	/// @see powerOff, restoreState
	///
	/// @return True, if success
	///
	virtual bool switchOff();

	void blinking(QJsonObject params);

signals:
	///
	/// @brief Emits whenever the LED-Device switches between on/off.
	///
	/// @param[in] newState The new state of the device
	///
	void enableStateChanged(bool newState);

	void manualUpdate();

	void newCounter(PerformanceReport pr);

protected:

	///
	/// @brief Initialise the device's configuration.
	///
	/// @param[in] deviceConfig the JSON device configuration
	/// @return True, if success
	///
	virtual bool init(const QJsonObject& deviceConfig);

	///
	/// @brief Opens the output device.
	///
	/// @return Zero, on success (i.e. device is ready), else negative
	///
	virtual int open();

	///
	/// @brief Closes the output device.
	///
	/// @return Zero on success (i.e. device is closed), else negative
	///
	virtual int close();

	///
	/// @brief Writes the RGB-Color values to the LEDs.
	///
	/// @param[in] ledValues The RGB-color per LED
	/// @return Zero on success, else negative
	///
	virtual int write(const std::vector<ColorRgb>& ledValues) = 0;

	///
	/// @brief Writes "BLACK" to the output stream,
	/// even if the device is not in enabled state (allowing to have a defined state during device power-off).
	/// @note: latch-time is considered between each write
	///
	/// @param[in] numberOfWrites Write Black given number of times
	/// @return Zero on success else negative
	///
	virtual int writeBlack(int numberOfBlack = 2);

	///
	/// @brief Power-/turn on the LED-device.
	///
	/// Powers-/Turns on the LED hardware, if supported.
	///
	/// @return True, if success
	///
	virtual bool powerOn();

	///
	/// @brief Power-/turn off the LED-device.
	///
	/// Depending on the device's capability, the device is powered-/turned off or
	/// an off state is simulated by writing "Black to LED" (default).
	///
	/// @return True, if success
	///
	virtual bool powerOff();

	///
	/// @brief Store the device's original state.
	///
	/// Save the device's state before hyperhdr color streaming starts allowing to restore state during switchOff().
	///
	/// @return True, if success
	///
	virtual bool storeState();

	///
	/// @brief Restore the device's original state.
	///
	/// Restore the device's state as before hyperhdr color streaming started.
	/// This includes the on/off state of the device.
	///
	/// @return True, if success
	///
	virtual bool restoreState();

	///
	/// @brief Converts an uint8_t array to hex string.
	///
	/// @param data uint8_t array
	/// @param size of the array
	/// @param number Number of array items to be converted.
	/// @return array as string of hex values
	QString uint8_t_to_hex_string(const uint8_t* data, const int size, int number = -1) const;

	///
	/// @brief Converts a ByteArray to hex string.
	///
	/// @param data ByteArray
	/// @param number Number of array items to be converted.
	/// @return array as string of hex values
	QString toHex(const QByteArray& data, int number = -1) const;

	/// Current device's type
	QString _activeDeviceType;

	/// Helper to pipe device configuration from constructor to start()
	QJsonObject _devConfig;

	/// The common Logger instance for all LED-devices
	Logger* _log;

	/// The buffer containing the packed RGB values
	std::vector<uint8_t> _ledBuffer;

	/// Timer object which makes sure that LED data is written at a minimum rate
	/// e.g. some devices will switch off when they do not receive data at least every 15 seconds
	QTimer* _refreshTimer;

	// Device configuration parameters

	/// Refresh interval in milliseconds
	int _refreshTimerInterval_ms;

	/// Number of hardware LEDs supported by device.
	uint _ledCount;
	uint _ledRGBCount;
	uint _ledRGBWCount;

	/// Does the device allow restoring the original state?
	bool	_isRestoreOrigState;

	/// Device, lights state before streaming via hyperhdr
	QJsonObject _orignalStateValues;

	// Device states
	/// Is the device enabled?
	bool _isEnabled;

	/// Is the device initialised?
	bool _isDeviceInitialised;

	/// Is the device ready for processing?
	bool _isDeviceReady;

	/// Is the device switched on?
	bool _isOn;

	/// Is the device in error state and stopped?
	bool _isDeviceInError;

	bool	_retryMode;
	int		_maxRetry;
	int		_currentRetry;

	static std::atomic<bool> _signalTerminate;

protected slots:

	///
	/// @brief Write the last data to the LEDs again.
	///
	/// @return Zero on success else negative
	///
	int rewriteLEDs();

	///
	/// @brief Set device in error state
	///
	/// @param[in] errorMsg The error message to be logged
	///
	virtual void setInError(const QString& errorMsg);

protected:
	void enableDevice(bool toEmit);
	void disableDevice(bool toEmit);	
	void startRefreshTimer();

private:

	/// @brief Stop refresh cycle
	void stopRefreshTimer();

	/// Is last write refreshing enabled?
	bool	_isRefreshEnabled;

	bool	_newFrame2Send;
	int64_t _newFrame2SendTime;

	/// Last LED values written
	std::vector<ColorRgb> _lastLedValues;

	struct
	{
		qint64		token = 0;
		qint64		statBegin = 0;
		qint64		frames = 0;
		qint64		incomingframes = 0;
		qint64		droppedFrames = 0;
	} _computeStats;

	int _blinkIndex;
};

#endif // LEDEVICE_H
