#include <leddevice/LedDevice.h>

//QT include
#include <QResource>
#include <QStringList>
#include <QDir>
#include <QTimer>

#include <base/HyperHdrInstance.h>
#include <utils/JsonUtils.h>

//std includes
#include <sstream>
#include <iomanip>

LedDevice::LedDevice(const QJsonObject& deviceConfig, QObject* parent)
	: QObject(parent)
	, _devConfig(deviceConfig)
	, _log(Logger::getInstance("LEDDEVICE_" + deviceConfig["type"].toString("unknown").toUpper()))
	, _ledBuffer(0)
	, _refreshTimer(nullptr)
	, _refreshTimerInterval_ms(0)
	, _ledCount(0)
	, _isRestoreOrigState(false)
	, _isEnabled(false)
	, _isDeviceInitialised(false)
	, _isDeviceReady(false)
	, _isOn(false)
	, _isDeviceInError(false)
	, _retryMode(false)
	, _maxRetry(60)
	, _currentRetry(0)
	, _isRefreshEnabled(false)
	, _newFrame2Send(false)
	, _newFrame2SendTime(0)
	, _blinkIndex(-1)
{
	_activeDeviceType = deviceConfig["type"].toString("UNSPECIFIED").toLower();

	connect(this, &LedDevice::manualUpdate, this, &LedDevice::rewriteLEDs, Qt::QueuedConnection);
}

LedDevice::~LedDevice()
{
	stopRefreshTimer();
}

void LedDevice::start()
{
	Info(_log, "Start LedDevice '%s'.", QSTRING_CSTR(_activeDeviceType));

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

void LedDevice::stop()
{
	Debug(_log, "Stop device");

	this->disable();
	this->stopRefreshTimer();
	Info(_log, " Stopped LedDevice '%s'", QSTRING_CSTR(_activeDeviceType));
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

void LedDevice::setInError(const QString& errorMsg)
{
	_isOn = false;
	_isDeviceInError = true;
	_isDeviceReady = false;
	_isEnabled = false;
	this->stopRefreshTimer();

	Error(_log, "Device disabled, device '%s' signals error: '%s'", QSTRING_CSTR(_activeDeviceType), QSTRING_CSTR(errorMsg));
	emit enableStateChanged(_isEnabled);
}

void LedDevice::enable()
{
	Debug(_log, "Enable device");
	enableDevice(true);
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

				if (toEmit)
					emit enableStateChanged(_isEnabled);
			}
		}

		if (_isRefreshEnabled)
			this->startRefreshTimer();
	}

	_newFrame2Send = false;
	_blinkIndex = -1;
}

void LedDevice::disable()
{
	Debug(_log, "Disable device");
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
			emit enableStateChanged(_isEnabled);
	}

	_newFrame2Send = false;
	_blinkIndex = -1;
}

void LedDevice::setActiveDeviceType(const QString& deviceType)
{
	_activeDeviceType = deviceType;
}

bool LedDevice::init(const QJsonObject& deviceConfig)
{
	Debug(_log, "deviceConfig: [%s]", QString(QJsonDocument(_devConfig).toJson(QJsonDocument::Compact)).toUtf8().constData());

	setLedCount(deviceConfig["currentLedCount"].toInt(1)); // property injected to reflect real led count
	setRefreshTime(deviceConfig["refreshTime"].toInt(_refreshTimerInterval_ms));

	return true;
}

void LedDevice::startRefreshTimer()
{
	if (_isDeviceReady && _isEnabled && _refreshTimerInterval_ms > 0)
	{
		// setup refreshTimer
		if (_refreshTimer == nullptr)
		{
			_refreshTimer = new QTimer(this);
			_refreshTimer->setTimerType(Qt::PreciseTimer);
			_refreshTimer->setInterval(_refreshTimerInterval_ms);
			connect(_refreshTimer, &QTimer::timeout, this, &LedDevice::rewriteLEDs);
		}
		else
			_refreshTimer->setInterval(_refreshTimerInterval_ms);

		Debug(_log, "Starting timer with interval = %ims", _refreshTimer->interval());

		_refreshTimer->start();
	}
	else if (_refreshTimerInterval_ms > 0)
	{
		Debug(_log, "Device is not ready to start a timer");
	}
}

void LedDevice::stopRefreshTimer()
{
	if (_refreshTimer != nullptr)
	{
		Debug(_log, "Stopping timer");

		_refreshTimer->stop();
		delete _refreshTimer;
		_refreshTimer = nullptr;		
	}
}

void LedDevice::setRefreshTime(int refreshTime_ms)
{
	_refreshTimerInterval_ms = qMax(refreshTime_ms, 0);

	if (_refreshTimerInterval_ms > 0)
	{

		_isRefreshEnabled = true;

		Debug(_log, "Refresh interval = %dms", _refreshTimerInterval_ms);

		startRefreshTimer();
	}
	else
	{
		_isRefreshEnabled = false;
		stopRefreshTimer();
	}

	Debug(_log, "RefreshTime updated to %dms", _refreshTimerInterval_ms);
}

int LedDevice::updateLeds(std::vector<ColorRgb> ledValues)
{
	// stats
	int64_t now = InternalClock::now();
	int64_t diff = now - _computeStats.statBegin;
	int64_t prevToken = _computeStats.token;

	if (_computeStats.token <= 0 || diff < 0)
	{
		_computeStats.token = PerformanceCounters::currentToken();
		_computeStats.statBegin = now;
		_computeStats.frames = 0;
		_computeStats.droppedFrames = 0;
		_computeStats.incomingframes = 1;
	}
	else if (prevToken != (_computeStats.token = PerformanceCounters::currentToken()))
	{
		if (_isRefreshEnabled && _refreshTimerInterval_ms > 0)
		{
			qint64 wanted = (1000.0/_refreshTimerInterval_ms) * 60.0 * diff / 60000.0;
			_computeStats.droppedFrames = std::max(wanted - _computeStats.frames - 1, 0ll);
		}

		if (diff >= 59000 && diff <= 65000)
			emit this->newCounter(
			PerformanceReport(static_cast<int>(PerformanceReportType::LED), _computeStats.token, this->_activeDeviceType, _computeStats.frames / qMax(diff / 1000.0, 1.0), _computeStats.frames, _computeStats.incomingframes, _computeStats.droppedFrames));

		_computeStats.statBegin = now;
		_computeStats.frames = 0;
		_computeStats.droppedFrames = 0;
		_computeStats.incomingframes = 1;
	}
	else
		_computeStats.incomingframes++;


	if (!_isEnabled || !_isOn || !_isDeviceReady || _isDeviceInError)
	{
		return -1;
	}
	else
	{
		if (_blinkIndex < 0)
			_lastLedValues = ledValues;

		if (!_isRefreshEnabled && (!_newFrame2Send || now - _newFrame2SendTime > 1000 || now < _newFrame2SendTime))
		{
			_newFrame2Send = true;
			_newFrame2SendTime = now;
			emit manualUpdate();
		}
		else if (!_isRefreshEnabled)
			_computeStats.droppedFrames++;
	}

	return 0;
}

int LedDevice::rewriteLEDs()
{
	int retval = -1;

	_newFrame2Send = false;

	if (_isEnabled && _isOn && _isDeviceReady && !_isDeviceInError)
	{
		if (_lastLedValues.size() > 0 )
			retval = write(_lastLedValues);

		_computeStats.frames++;
	}
	
	return retval;
}

int LedDevice::writeBlack(int numberOfBlack)
{
	int rc = -1;

	Debug(_log, "Set LED strip to black/power off");

	_lastLedValues = std::vector<ColorRgb>(static_cast<unsigned long>(_ledCount), ColorRgb::BLACK);

	for (int i = 0; i < numberOfBlack; i++)
	{		
		rc = write(_lastLedValues);
	}

	return rc;
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

void LedDevice::setLedCount(int ledCount)
{
	assert(ledCount >= 0);
	_ledCount = ledCount;
	_ledRGBCount = _ledCount * sizeof(ColorRgb);
	_ledRGBWCount = _ledCount * sizeof(ColorRgbw);
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
	return _refreshTimerInterval_ms;
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

void LedDevice::identifyLed(const QJsonObject& params)
{
	_blinkIndex = params["blinkIndex"].toInt(-1);

	if (_blinkIndex < 0 || _blinkIndex >= _lastLedValues.size())
	{
		_blinkIndex = -1;
	}
	else
	{
		for (auto iter = _lastLedValues.begin(); iter != _lastLedValues.end(); ++iter)
		{
			*iter = ColorRgb::BLACK;
		}

		for (int i = 0; i < 6; i++)
		{
			const int blinkOrg = _blinkIndex;

			ColorRgb color = ColorRgb::BLUE;
			if (i % 3 == 0)
				color = ColorRgb::RED;
			else if (i % 3 == 1)
				color = ColorRgb::GREEN;

			QTimer::singleShot(800 * i, this, [this, color, blinkOrg]() {
				if (_blinkIndex == blinkOrg && _blinkIndex >= 0 && _blinkIndex < _lastLedValues.size())
				{
					_lastLedValues[_blinkIndex] = color;
					rewriteLEDs();
				}
			});
		}
		QTimer::singleShot(4800, this, [this]() { _blinkIndex = -1; });
	}
}


