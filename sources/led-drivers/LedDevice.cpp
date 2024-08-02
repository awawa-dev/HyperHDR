#ifndef PCH_ENABLED
	#include <QResource>
	#include <QStringList>
	#include <QDir>
	#include <QTimer>

	#include <sstream>
	#include <iomanip>
#endif

#include <led-drivers/LedDevice.h>
#include <base/HyperHdrInstance.h>
#include <utils/GlobalSignals.h>

std::atomic<bool> LedDevice::_signalTerminate(false);

LedDevice::LedDevice(const QJsonObject& deviceConfig, QObject* parent)
	: QObject(parent)
	, _devConfig(deviceConfig)
	, _log(Logger::getInstance("LEDDEVICE_" + deviceConfig["type"].toString("unknown").toUpper()))
	, _ledBuffer(0)
	, _refreshTimer(nullptr)
	, _currentInterval(0)
	, _defaultInterval(0)
	, _forcedInterval(0)
	, _smoothingInterval(0)
	, _ledCount(0)
	, _isRestoreOrigState(false)
	, _isEnabled(false)
	, _isDeviceInitialised(false)
	, _isDeviceReady(false)
	, _isOn(false)
	, _isDeviceInError(false)
	, _maxRetry(60)
	, _currentRetry(0)
	, _retryTimer(nullptr)
	, _isRefreshEnabled(false)
	, _newFrame2Send(false)
	, _blinkIndex(-1)
	, _blinkTime(0)
	, _instanceIndex(-1)
	, _pauseRetryTimer(-1)
{
	std::shared_ptr<DiscoveryWrapper> discoveryWrapper = nullptr;
	emit GlobalSignals::getInstance()->SignalGetDiscoveryWrapper(discoveryWrapper);
	_discoveryWrapper = discoveryWrapper;

	_activeDeviceType = deviceConfig["type"].toString("UNSPECIFIED").toLower();

	connect(this, &LedDevice::SignalManualUpdate, this, &LedDevice::rewriteLEDs, Qt::QueuedConnection);
}

LedDevice::~LedDevice()
{
	stopRefreshTimer();
	stopRetryTimer();
}

bool LedDevice::switchOn()
{
	bool rc = false;

	Debug(_log, "Switch on");

	if (_isOn)
	{
		rc = true;
	}
	else
	{
		if (_isDeviceInitialised)
		{
			storeState();

			if (powerOn())
			{
				_isOn = true;
				rc = true;
			}
		}
	}
	return rc;
}

bool LedDevice::switchOff()
{
	bool rc = false;

	Debug(_log, "Switch off");

	if (!_isOn && !_isRestoreOrigState)
	{
		rc = true;
	}
	else
	{
		if (_isDeviceInitialised)
		{
			// Disable device to ensure no standard Led updates are written/processed
			_isOn = false;

			rc = true;

			if (_isDeviceReady)
			{
				if (_isRestoreOrigState)
				{
					//Restore devices state
					restoreState();
				}
				else
				{
					powerOff();
				}
			}
		}
	}
	return rc;
}

bool LedDevice::powerOff()
{
	bool rc = false;

	Debug(_log, "Power Off");

	// Simulate power-off by writing a final "Black" to have a defined outcome
	if (writeBlack() >= 0)
	{
		rc = true;
	}
	return rc;
}

bool LedDevice::powerOn()
{
	bool rc = true;

	Debug(_log, "Power On");

	return rc;
}

void LedDevice::setInstanceIndex(int instanceIndex)
{
	_instanceIndex = instanceIndex;
	_log = Logger::getInstance(QString("LEDDEVICE%1_%2").arg(_instanceIndex).arg(_activeDeviceType.toUpper()));
}

int LedDevice::open()
{
	_isDeviceReady = true;
	int retval = 0;

	return retval;
}

int LedDevice::close()
{
	_isDeviceReady = false;
	int retval = 0;

	return retval;
}

void LedDevice::setupRetry(int interval)
{
	if (_retryTimer == nullptr && _maxRetry > 0)
	{
		_currentRetry = _maxRetry;
		_retryTimer = std::unique_ptr<QTimer>(new QTimer());
		connect(_retryTimer.get(), &QTimer::timeout, this, [this](){
				if (_currentRetry > 0 && !_signalTerminate)
				{
					Warning(_log, "The LED device is not ready... trying to reconnect (try %i/%i).", (_maxRetry - _currentRetry + 1), _maxRetry);
					_currentRetry--;
					enableDevice(true);
				}
				else
				{
					Error(_log, "The LED device is not ready... give up.");					
					stopRetryTimer();
				}
			});
		_retryTimer->start(interval);
	}
}

void LedDevice::start()
{
	Info(_log, "Start LedDevice '%s'.", QSTRING_CSTR(_activeDeviceType));

	if (_isDeviceInitialised)
		close();

	_isDeviceInitialised = false;
	// General initialisation and configuration of LedDevice
	if (init(_devConfig))
	{
		// Everything is OK -> enable device
		_isDeviceInitialised = true;
		this->enable();
	}
}

void LedDevice::pauseRetryTimer(bool mode)
{
	if (mode)
	{
		if (_pauseRetryTimer < 0)
		{
			_pauseRetryTimer = (_retryTimer != nullptr && _retryTimer->isActive()) ? _retryTimer->interval() : 0;
			Debug(_log, "Saving retryTimer (interval: %i)", _pauseRetryTimer);
			stopRetryTimer();
		}
	}
	else
	{
		if (_pauseRetryTimer >= 0)
		{
			Debug(_log, "Restoring retryTimer (interval: %i)", _pauseRetryTimer);
			if (_pauseRetryTimer > 0 && !_isOn)
			{
				setupRetry(_pauseRetryTimer);
			}
			_pauseRetryTimer = -1;
		}
	}
}

void LedDevice::enable()
{
	if (!_signalTerminate)
	{
		_pauseRetryTimer = -1;
		stopRetryTimer();

		if (!_isDeviceInitialised && init(_devConfig))
		{
			Debug(_log, "First enable the device");
			_isDeviceInitialised = true;
		}

		if (_isDeviceInitialised)
		{
			Debug(_log, "Enable the device");
			enableDevice(true);
		}
		else
			Error(_log, "Could not initialize the device before enabling");
	}
}

void LedDevice::enableDevice(bool toEmit)
{
	if (!_isEnabled)
	{
		_isDeviceInError = false;

		if (!_isDeviceReady)
		{
			open();
		}

		if (_isDeviceReady)
		{
			if (switchOn())
			{
				_isDeviceReady = true;
				_isEnabled = true;
				stopRetryTimer();

				if (toEmit)
					emit SignalEnableStateChanged(_isEnabled);
			}
		}

		if (_isRefreshEnabled)
			this->startRefreshTimer();
	}
}

void LedDevice::stop()
{
	Debug(_log, "Stop device");
	
	stopRefreshTimer();
	stopRetryTimer();
	disable();

	Info(_log, " Stopped LedDevice '%s'", QSTRING_CSTR(_activeDeviceType));
}

void LedDevice::disable()
{
	Debug(_log, "Disable the device");	
	disableDevice(true);
}

void LedDevice::disableDevice(bool toEmit)
{
	if (_isEnabled)
	{
		_isEnabled = false;

		if (_isRefreshEnabled)
			this->stopRefreshTimer();

		switchOff();
		close();

		if (toEmit)
			emit SignalEnableStateChanged(_isEnabled);
	}
}

void LedDevice::setInError(const QString& errorMsg)
{
	_isOn = false;
	_isDeviceInError = true;
	_isDeviceReady = false;
	_isEnabled = false;
	this->stopRefreshTimer();

	Error(_log, "Device '%s' is disabled due to an error: '%s'", QSTRING_CSTR(_activeDeviceType), QSTRING_CSTR(errorMsg));
	emit SignalEnableStateChanged(_isEnabled);
}

void LedDevice::setActiveDeviceType(const QString& deviceType)
{
	_activeDeviceType = deviceType;
}

bool LedDevice::init(const QJsonObject& deviceConfig)
{
	Debug(_log, "deviceConfig: [%s]", QString(QJsonDocument(_devConfig).toJson(QJsonDocument::Compact)).toUtf8().constData());

	_defaultInterval = deviceConfig["refreshTime"].toInt(0);
	_forcedInterval = deviceConfig["forcedRefreshTime"].toInt(0);
	_smoothingInterval = deviceConfig["smoothingRefreshTime"].toInt(0);

	setLedCount(deviceConfig["currentLedCount"].toInt(1)); // property injected to reflect real led count
	setRefreshTime(deviceConfig["refreshTime"].toInt(_currentInterval));

	return true;
}

void LedDevice::startRefreshTimer()
{
	if (_isDeviceReady && _isEnabled && _currentInterval > 0)
	{
		// setup refreshTimer
		if (_refreshTimer == nullptr)
		{
			_refreshTimer = std::unique_ptr<QTimer>(new QTimer());
			_refreshTimer->setTimerType(Qt::PreciseTimer);
			_refreshTimer->setInterval(_currentInterval);
			if (_smoothingInterval > 0)
				connect(_refreshTimer.get(), &QTimer::timeout, this, &LedDevice::SignalSmoothingClockTick, Qt::UniqueConnection);
			else
				connect(_refreshTimer.get(), &QTimer::timeout, this, &LedDevice::rewriteLEDs, Qt::UniqueConnection);
		}
		else
			_refreshTimer->setInterval(_currentInterval);

		Debug(_log, "Starting timer with interval = %ims", _refreshTimer->interval());

		_refreshTimer->start();
	}
	else if (_currentInterval > 0)
	{
		Debug(_log, "Device is not ready to start a timer");
	}
}

void LedDevice::stopRefreshTimer()
{
	if (_refreshTimer != nullptr)
	{
		Debug(_log, "Stopping refresh timer");

		_refreshTimer->stop();
		_refreshTimer.reset();
	}
}

void LedDevice::stopRetryTimer()
{
	if (_retryTimer != nullptr)
	{
		Debug(_log, "Stopping retry timer");

		_retryTimer->stop();
		_retryTimer.reset();
	}
}

void LedDevice::setRefreshTime(int userInterval)
{
	int selectedInterval = userInterval;

	if (_forcedInterval > 0)
	{
		selectedInterval = _forcedInterval;
		if (userInterval > 0)
			Warning(_log, "Ignoring user LED refresh rate. Forcing device specific refresh rate = %.2f Hz", (1000.0/selectedInterval));
	}
	else if (_smoothingInterval > 0)
	{
		selectedInterval = _smoothingInterval;
		if (userInterval > 0)
			Warning(_log, "Ignoring user LED refresh rate. Forcing smoothing refresh rate = %.2f Hz", (1000.0/selectedInterval));
	}

	_currentInterval = qMax(selectedInterval, 0);

	if (_currentInterval > 0)
	{

		_isRefreshEnabled = true;

		Debug(_log, "Refresh rate = %.2f Hz", (1000.0/_currentInterval));

		startRefreshTimer();
	}
	else
	{
		_isRefreshEnabled = false;
		stopRefreshTimer();
	}

	Debug(_log, "Refresh interval updated to %dms", _currentInterval);
}

int LedDevice::hasLedClock()
{
	return _forcedInterval;
}

void LedDevice::smoothingRestarted(int newSmoothingInterval)
{
	if (_smoothingInterval != newSmoothingInterval)
	{
		_smoothingInterval = newSmoothingInterval;
		stopRefreshTimer();
		setRefreshTime(_defaultInterval);
		Debug(_log, "LED refresh interval adjustment caused by smoothing configuration change to %dms (proposed: %dms)", _currentInterval, newSmoothingInterval);
	}
}

void LedDevice::updateLeds(std::vector<ColorRgb> ledValues)
{
	// stats
	int64_t now = InternalClock::now();
	int64_t diff = now - _computeStats.statBegin;
	int64_t prevToken = _computeStats.token;

	if (_computeStats.token <= 0 || diff < 0)
	{
		_computeStats.token = PerformanceCounters::currentToken();
		_computeStats.reset(now);
	}
	else if (prevToken != (_computeStats.token = PerformanceCounters::currentToken()))
	{
		if (_isRefreshEnabled && _currentInterval > 0)
		{
			qint64 wanted = (1000.0/_currentInterval) * 60.0 * diff / 60000.0;
			_computeStats.droppedFrames = std::max(wanted - _computeStats.frames - 1, 0ll);
		}

		if (diff >= 59000 && diff <= 65000)
			emit GlobalSignals::getInstance()->SignalPerformanceNewReport(
				PerformanceReport(hyperhdr::PerformanceReportType::LED, _computeStats.token, this->_activeDeviceType + _customInfo, _computeStats.frames / qMax(diff / 1000.0, 1.0), _computeStats.frames, _computeStats.incomingframes, _computeStats.droppedFrames, _instanceIndex));

		_computeStats.reset(now);
	}
	else
		_computeStats.incomingframes++;


	if (!_isEnabled || !_isOn || !_isDeviceReady || _isDeviceInError)
	{
		return;
	}
	else
	{
		_lastLedValues = ledValues;

		if (!_isRefreshEnabled)
		{
			if (_newFrame2Send)
				_computeStats.droppedFrames++;
			_newFrame2Send = true;
			emit SignalManualUpdate();
		}
		else if (QThread::currentThread() == this->thread())
			rewriteLEDs();
	}

	return;
}

int LedDevice::rewriteLEDs()
{
	int retval = -1;

	if ((_newFrame2Send || _isRefreshEnabled) && _isEnabled && _isOn && _isDeviceReady && !_isDeviceInError)
	{
		_newFrame2Send = false;

		std::vector<ColorRgb> copy = _lastLedValues;

		if (_signalTerminate)
			copy = std::vector<ColorRgb>(static_cast<unsigned long>(copy.size()), ColorRgb::BLACK);
		else if (_blinkIndex >= 0)
		{
			int64_t now = InternalClock::now();
			if (_blinkTime + 4500 < now || _blinkTime > now || _blinkIndex >= static_cast<int>(copy.size()))
				_blinkIndex = -1;
			else
			{
				copy = std::vector<ColorRgb>(static_cast<unsigned long>(copy.size()), ColorRgb::BLACK);
				copy[_blinkIndex] = (_blinkTime + 1500 > now) ? ColorRgb::RED : (_blinkTime + 3000 > now) ? ColorRgb::GREEN : ColorRgb::BLUE;
			}
		}		

		if (copy.size() > 0)
			retval = write(copy);

		if (_signalTerminate)
			disableDevice(false);

		_computeStats.frames++;
	}

	return retval;
}

int LedDevice::writeBlack(int numberOfBlack)
{
	int rc = -1;
	
	Debug(_log, "Set LED strip to black/power off");

	std::vector<ColorRgb> blacks = std::vector<ColorRgb>(static_cast<unsigned long>(_ledCount), ColorRgb::BLACK);

	_lastLedValues = blacks;

	for (int i = 0; i < numberOfBlack; i++)
	{
		rc = write(blacks);
	}

	return rc;
}

void LedDevice::setLedCount(int ledCount)
{
	assert(ledCount >= 0);
	_ledCount = ledCount;
	_ledRGBCount = _ledCount * sizeof(ColorRgb);
	_ledRGBWCount = _ledCount * sizeof(ColorRgbw);
}

QJsonObject LedDevice::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;

	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

	QJsonArray deviceList;
	devicesDiscovered.insert("devices", deviceList);

	Debug(_log, "devicesDiscovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());
	return devicesDiscovered;
}

QString LedDevice::discoverFirst()
{
	QString deviceDiscovered;

	Debug(_log, "deviceDiscovered: [%s]", QSTRING_CSTR(deviceDiscovered));

	return deviceDiscovered;
}

void LedDevice::identifyLed(const QJsonObject& params)
{
	_blinkIndex = params["blinkIndex"].toInt(-1);

	if (_blinkIndex < 0)
	{
		_blinkIndex = -1;
	}
	else
	{
		_newFrame2Send = true;
		_blinkTime = InternalClock::now();
		rewriteLEDs();
	}
}

void LedDevice::signalTerminateTriggered()
{
	_signalTerminate = true;
}

void LedDevice::blinking(QJsonObject params)
{
	if (params["blinkIndex"].toInt(-1) != -1)
		identifyLed(params);
	else
		identify(params);
}

QJsonObject LedDevice::getProperties(const QJsonObject& params)
{
	QJsonObject properties;
	QJsonObject deviceProperties;

	Debug(_log, "params: [%s]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());

	properties.insert("properties", deviceProperties);

	Debug(_log, "properties: [%s]", QString(QJsonDocument(properties).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return properties;
}

bool LedDevice::storeState()
{
	bool rc = true;

	if (_isRestoreOrigState)
	{
		// Save device's original state
		// _originalStateValues = get device's state;
		// store original power on/off state, if available
	}
	return rc;
}

bool LedDevice::restoreState()
{
	bool rc = true;

	if (_isRestoreOrigState)
	{
		// Restore device's original state
		// update device using _originalStateValues
		// update original power on/off state, if supported
	}
	return rc;
}

void LedDevice::printLedValues(const std::vector<ColorRgb>& ledValues)
{
	std::cout << "LedValues [" << ledValues.size() << "] [";
	for (const ColorRgb& color : ledValues)
	{
		std::cout << color;
	}
	std::cout << "]" << std::endl;
}

QString LedDevice::uint8_t_to_hex_string(const uint8_t* data, const int size, int number) const
{
	if (number <= 0 || number > size)
	{
		number = size;
	}

	QByteArray bytes(reinterpret_cast<const char*>(data), number);

	return bytes.toHex(':');
}

QString LedDevice::toHex(const QByteArray& data, int number) const
{
	if (number <= 0 || number > data.size())
	{
		number = data.size();
	}

	return data.left(number).toHex(':');
}

bool LedDevice::isInitialised() const {
	return _isDeviceInitialised;
}

bool LedDevice::isReady() const {
	return _isDeviceReady;
}

bool LedDevice::isInError() const {
	return _isDeviceInError;
}

int LedDevice::getRefreshTime() const {
	return _currentInterval;
}

int LedDevice::getLedCount() const {
	return _ledCount;
}

QString LedDevice::getActiveDeviceType() const {
	return _activeDeviceType;
}

bool LedDevice::componentState() const {
	return _isEnabled;
}

void LedDevice::LedStats::reset(int64_t now)
{
	statBegin = now;
	frames = 0;
	droppedFrames = 0;
	incomingframes = 1;
}
