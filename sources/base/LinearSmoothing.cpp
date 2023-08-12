// Qt includes
#include <QTimer>
#include <QThread>

#include <base/LinearSmoothing.h>
#include <base/HyperHdrInstance.h>

#include <cmath>
#include <stdint.h>
#include <inttypes.h>

using namespace hyperhdr;

const int64_t  DEFAUL_SETTLINGTIME		= 200;   // settlingtime in ms
const double   DEFAUL_UPDATEFREQUENCY	= 25;    // updatefrequncy in hz
const double   MINIMAL_UPDATEFREQUENCY	= 20;



LinearSmoothing::LinearSmoothing(const QJsonDocument& config, HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr),
	_log(Logger::getInstance(QString("SMOOTHING%1").arg(hyperhdr->getInstanceIndex()))),
	_hyperhdr(hyperhdr),
	_updateInterval(static_cast<int64_t>(1000 / DEFAUL_UPDATEFREQUENCY)),
	_settlingTime(DEFAUL_SETTLINGTIME),
	_timer(nullptr),
	_continuousOutput(false),
	_antiFlickeringTreshold(0),
	_antiFlickeringStep(0),
	_antiFlickeringTimeout(0),
	_flushFrame(false),
	_targetTime(0),
	_previousTime(0),
	_pause(false),
	_currentConfigId(0),
	_enabled(false),
	_directMode(false),
	_smoothingType(SmoothingType::Linear),
	_infoUpdate(true),
	_infoInput(true),
	_coolDown(0),
	_debugCounter(0)
{
	// timer
	_timer = new QTimer(this);
	_timer->setTimerType(Qt::PreciseTimer);

	// init cfg 0 (default)
	addConfig(DEFAUL_SETTLINGTIME, DEFAUL_UPDATEFREQUENCY);
	handleSettingsUpdate(settings::type::SMOOTHING, config);
	selectConfig(0, true);

	// add pause on cfg 1
	SmoothingCfg cfg(true, 0, 0, false);
	_cfgList.push_back(cfg);

	// listen for comp changes
	connect(_hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &LinearSmoothing::componentStateChange);
	connect(_timer, &QTimer::timeout, this, &LinearSmoothing::updateLeds);
}

LinearSmoothing::~LinearSmoothing()
{
	delete _timer;
}

inline uint8_t LinearSmoothing::clamp(int x)
{
	return (x < 0) ? 0 : ((x > 255) ? 255 : uint8_t(x));
}

void LinearSmoothing::clearQueuedColors(bool deviceEnabled, bool restarting)
{
	try
	{
		Info(_log, "Clearing queued colors before: %s%s",
			(deviceEnabled) ? "enabling" : "disabling",
			(restarting) ? ". Smoothing configuration changed: restarting timer." : "");

		if (!deviceEnabled || restarting)
		{
			_timer->stop();
		}

		_timer->setInterval(_updateInterval);
		_previousValues.clear();
		_previousTimeouts.clear();
		_previousTime = 0;
		_targetValues.clear();
		_targetTime = 0;
		_flushFrame = false;
		_infoUpdate = true;
		_infoInput = true;
		_coolDown = 0;

		if (deviceEnabled)
		{
			_timer->start();
		}

		Info(_log, "Smoothing queue is cleared");
	}
	catch (...)
	{
		Debug(_log, "Smoothing error detected");
	}
}

void LinearSmoothing::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::SMOOTHING)
	{
		if (InternalClock::isPreciseSteady())
			Info(_log, "High resolution clock is steady (good)");
		else
			Warning(_log, "High resolution clock is NOT STEADY!");

		QJsonObject obj = config.object();

		if (enabled() != obj["enable"].toBool(true))
		{
			setEnable(obj["enable"].toBool(true));
		}

		_continuousOutput = obj["continuousOutput"].toBool(true);

		SmoothingCfg	cfg(false,
			static_cast<int64_t>(obj["time_ms"].toInt(DEFAUL_SETTLINGTIME)),
			static_cast<int64_t>(std::max(1000.0 / std::max(obj["updateFrequency"].toDouble(DEFAUL_UPDATEFREQUENCY), MINIMAL_UPDATEFREQUENCY), 5.0)),
			false);

		auto smoothingType = obj["type"].toString("linear");

		if (smoothingType == "alternative")
			cfg._type = SmoothingType::Alternative;
		else
			cfg._type = SmoothingType::Linear;

		cfg._antiFlickeringTreshold = obj["lowLightAntiFlickeringTreshold"].toInt(0);
		cfg._antiFlickeringStep = obj["lowLightAntiFlickeringValue"].toInt(0);
		cfg._antiFlickeringTimeout = obj["lowLightAntiFlickeringTimeout"].toInt(0);

		Info(_log, "Creating config (%d) => type: %s, dirMode: %s, pause: %s, settlingTime: %ims, interval: %ims (%iHz), antiFlickTres: %i, antiFlickStep: %i, antiFlickTime: %i",
			_currentConfigId, QSTRING_CSTR(SmoothingCfg::EnumToString(cfg._type)), (cfg._directMode) ? "true" : "false", (cfg._pause) ? "true" : "false", int(cfg._settlingTime), int(cfg._updateInterval), int(1000.0 / cfg._updateInterval), cfg._antiFlickeringTreshold, cfg._antiFlickeringStep, int(cfg._antiFlickeringTimeout));
		_cfgList[0] = cfg;

		// if current id is 0, we need to apply the settings (forced)
		if (_currentConfigId == 0)
		{
			selectConfig(0, true);
		}
	}
}

void LinearSmoothing::Antiflickering()
{
	if (_antiFlickeringTreshold > 0 && _antiFlickeringStep > 0 && _previousValues.size() == _targetValues.size() && _previousValues.size() == _previousTimeouts.size())
	{
		int64_t now = InternalClock::nowPrecise();

		for (size_t i = 0; i < _previousValues.size(); ++i)
		{
			auto	 newColor = _targetValues[i];
			auto     oldColor = _previousValues[i];
			int64_t& timeout = _previousTimeouts[i];

			int avVal = (std::min(int(newColor.red), std::min(int(newColor.green), int(newColor.blue))) +
				std::max(int(newColor.red), std::max(int(newColor.green), int(newColor.blue)))) / 2;

			if (avVal < _antiFlickeringTreshold)
			{
				int minR = std::abs(int(newColor.red) - int(oldColor.red));
				int minG = std::abs(int(newColor.green) - int(oldColor.green));
				int minB = std::abs(int(newColor.blue) - int(oldColor.blue));

				int select = std::max(std::max(minR, minG), minB);

				if (select < _antiFlickeringStep &&
					(newColor.red != 0 || newColor.green != 0 || newColor.blue != 0) &&
					(oldColor.red != 0 || oldColor.green != 0 || oldColor.blue != 0))
				{
					if (_antiFlickeringTimeout <= 0 || (now - timeout < _antiFlickeringTimeout && now > timeout))
						_targetValues[i] = _previousValues[i];
					else
						timeout = now;
				}
				else
					timeout = now;
			}
			else
				timeout = now;
		}
	}
}

void LinearSmoothing::updateLedValues(const std::vector<ColorRgb>& ledValues)
{
	if (!_enabled)
		return;

	_coolDown = 1;

	if (_directMode)
	{
		if (_timer->remainingTime() >= 0)
			clearQueuedColors();

		if (_pause || ledValues.size() == 0)
			return;

		queueColors(ledValues);

		return;
	}

	try
	{
		if (_infoInput)
			Info(_log, "Using %s smoothing input (%i)", ((_smoothingType == SmoothingType::Alternative) ? "alternative" : "linear"), _currentConfigId);

		_infoInput = false;
		LinearSetup(ledValues);
	}
	catch (...)
	{
		Debug(_log, "Smoothing error detected");
	}

	return;
}

void LinearSmoothing::LinearSetup(const std::vector<ColorRgb>& ledValues)
{
	_targetTime = InternalClock::nowPrecise() + _settlingTime;
	_targetValues = ledValues;

	/////////////////////////////////////////////////////////////////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!	

	if ((!_previousValues.empty() && (_previousValues.size() != _targetValues.size())) || _previousTime > _targetTime)
	{
		Warning(_log, "Detect %s has changed. Previuos value: %d, new value: %d", (_previousTime > _targetTime) ? "TIME" : "size", _previousValues.size(), _targetValues.size());
		_previousValues.clear();
		_previousTimeouts.clear();
	}

	if (_previousValues.empty())
	{
		_previousTime = InternalClock::nowPrecise();
		_previousValues = ledValues;
		_previousTimeouts.clear();
		_previousTimeouts.resize(_previousValues.size(), _previousTime);
	}

	Antiflickering();
}

inline uint8_t LinearSmoothing::computeColor(int64_t k, int color)
{
	int delta = std::abs(color);
	auto step = std::min((int)std::max(static_cast<int>((k * delta) >> 8), 1), delta);

	return step;
}

void LinearSmoothing::setupAdvColor(int64_t deltaTime, float& kOrg, float& kMin, float& kMid, float& kAbove, float& kMax)
{
	kOrg = std::max(1.0f - 1.0f * deltaTime / (_targetTime - _previousTime), 0.0001f);

	kMin = std::min(std::pow(kOrg, 1.0f), 1.0f);
	kMid = std::min(std::pow(kOrg, 0.9f), 1.0f);
	kAbove = std::min(std::pow(kOrg, 0.75f), 1.0f);
	kMax = std::min(std::pow(kOrg, 0.6f), 1.0f);
}

inline uint8_t LinearSmoothing::computeAdvColor(int limitMin, int limitAverage, int limitMax, float kMin, float kMid, float kAbove, float kMax, int color)
{
	int val = std::abs(color);
	if (val < limitMin)
		return std::ceil(kMax * val);
	else if (val < limitAverage)
		return std::ceil(kAbove * val);
	else if (val < limitMax)
		return std::ceil(kMid * val);
	else
		return std::ceil(kMin * val);
}

void LinearSmoothing::updateLeds()
{
	try
	{
		if (_smoothingType == SmoothingType::Alternative)
		{
			if (_infoUpdate)
				Info(_log, "Using alternative smoothing procedure (%i)", _currentConfigId);
			_infoUpdate = false;

			LinearSmoothingProcessing(true);
		}
		else
		{
			if (_infoUpdate)
				Info(_log, "Using linear smoothing procedure (%i)", _currentConfigId);
			_infoUpdate = false;

			LinearSmoothingProcessing(false);
		}
	}
	catch (...)
	{
		Debug(_log, "Smoothing error detected");
	}
}

void LinearSmoothing::queueColors(const std::vector<ColorRgb>& ledColors)
{
	if (!_pause)
	{
		emit _hyperhdr->ledDeviceData(ledColors);
	}
}

void LinearSmoothing::componentStateChange(hyperhdr::Components component, bool state)
{
	_flushFrame = state;

	if (component == hyperhdr::COMP_LEDDEVICE)
	{
		clearQueuedColors(state);
	}

	if (component == hyperhdr::COMP_SMOOTHING)
	{
		setEnable(state);
	}
}

void LinearSmoothing::setEnable(bool enable)
{
	_enabled = enable;

	clearQueuedColors(_enabled);

	// update comp register
	_hyperhdr->setNewComponentState(hyperhdr::COMP_SMOOTHING, enable);
}

unsigned LinearSmoothing::addConfig(int settlingTime_ms, double ledUpdateFrequency_hz, bool directMode)
{
	SmoothingCfg cfg(false, settlingTime_ms, int64_t(1000.0 / ledUpdateFrequency_hz), directMode);
	_cfgList.push_back(cfg);

	return (unsigned)(_cfgList.size() - 1);
}

unsigned LinearSmoothing::updateConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool directMode)
{
	unsigned updatedCfgID = cfgID;
	if (cfgID < static_cast<unsigned>(_cfgList.size()))
	{
		SmoothingCfg cfg(false, settlingTime_ms, int64_t(1000.0 / ledUpdateFrequency_hz), directMode);
		_cfgList[updatedCfgID] = cfg;
	}
	else
	{
		updatedCfgID = addConfig(settlingTime_ms, ledUpdateFrequency_hz, directMode);
	}

	return updatedCfgID;
}

void LinearSmoothing::updateCurrentConfig(int settlingTime_ms)
{
	_settlingTime = settlingTime_ms;

	Info(_log, "Updating current config (%d) => type: %s, dirMode: %s, pause: %s, settlingTime: %ims, interval: %ims (%iHz), antiFlickTres: %i, antiFlickStep: %i, antiFlickTime: %i",
		_currentConfigId, QSTRING_CSTR(SmoothingCfg::EnumToString(_smoothingType)), (_directMode) ? "true" : "false", (_pause) ? "true" : "false", int(_settlingTime),
		int(_updateInterval), int(1000.0 / _updateInterval), _antiFlickeringTreshold, _antiFlickeringStep, int(_antiFlickeringTimeout));
}

bool LinearSmoothing::selectConfig(unsigned cfg, bool force)
{
	//WATCH
	if (_currentConfigId == cfg && !force)
	{
		return true;
	}

	if (cfg < (unsigned)_cfgList.size())
	{
		_settlingTime = _cfgList[cfg]._settlingTime;
		_pause = _cfgList[cfg]._pause;
		_directMode = _cfgList[cfg]._directMode;
		_antiFlickeringTreshold = _cfgList[cfg]._antiFlickeringTreshold;
		_antiFlickeringStep = _cfgList[cfg]._antiFlickeringStep;
		_antiFlickeringTimeout = _cfgList[cfg]._antiFlickeringTimeout;

		int64_t newUpdateInterval = std::max(_cfgList[cfg]._updateInterval, (int64_t)5);

		if (newUpdateInterval != _updateInterval || _cfgList[cfg]._type != _smoothingType)
		{
			_updateInterval = newUpdateInterval;
			_smoothingType = _cfgList[cfg]._type;

			clearQueuedColors(!_pause && _enabled, true);
		}

		_currentConfigId = cfg;

		Info(_log, "Selecting config (%d) => type: %s, dirMode: %s, pause: %s, settlingTime: %ims, interval: %ims (%iHz), antiFlickTres: %i, antiFlickStep: %i, antiFlickTime: %i",
			_currentConfigId, QSTRING_CSTR(SmoothingCfg::EnumToString(_smoothingType)), (_directMode) ? "true" : "false", (_pause) ? "true" : "false", int(_settlingTime),
			int(_updateInterval), int(1000.0 / _updateInterval), _antiFlickeringTreshold, _antiFlickeringStep, int(_antiFlickeringTimeout));

		return true;
	}

	// reset to default
	_currentConfigId = 0;
	return false;
}

bool LinearSmoothing::pause() const
{
	return _pause;
}

bool LinearSmoothing::enabled() const
{
	return _enabled && !_pause;
}

LinearSmoothing::SmoothingCfg::SmoothingCfg() :
	_pause(false),
	_settlingTime(200),
	_updateInterval(25),
	_directMode(false),
	_type(SmoothingType::Linear),
	_antiFlickeringTreshold(0),
	_antiFlickeringStep(0),
	_antiFlickeringTimeout(0)
{
}

LinearSmoothing::SmoothingCfg::SmoothingCfg(bool pause, int64_t settlingTime, int64_t updateInterval, bool directMode, SmoothingType type, int antiFlickeringTreshold, int antiFlickeringStep, int64_t antiFlickeringTimeout) :
	_pause(pause),
	_settlingTime(settlingTime),
	_updateInterval(updateInterval),
	_directMode(directMode),
	_type(type),
	_antiFlickeringTreshold(antiFlickeringTreshold),
	_antiFlickeringStep(antiFlickeringStep),
	_antiFlickeringTimeout(antiFlickeringTimeout)
{
}

QString LinearSmoothing::SmoothingCfg::EnumToString(SmoothingType type)
{
	if (type == SmoothingType::Linear)
		return QString("Linear");
	else if (type == SmoothingType::Alternative)
		return QString("Alternative");

	return QString("Unknown");
}

void LinearSmoothing::LinearSmoothingProcessing(bool correction)
{
	float kOrg, kMin, kMid, kAbove, kMax;
	int64_t now = InternalClock::nowPrecise();
	int64_t deltaTime = _targetTime - now;
	int64_t k;

	int aspectLow = 16;
	int aspectMid = 32;
	int aspectHigh = 60;


	if (deltaTime <= 0 || _targetTime <= _previousTime)
	{
		_previousValues = _targetValues;
		_previousTimeouts.clear();
		_previousTimeouts.resize(_previousValues.size(), now);

		_previousTime = now;

		if (_flushFrame)
			queueColors(_previousValues);

		if (!_continuousOutput && _coolDown > 0)
		{
			_coolDown--;
			_flushFrame = true;
		}
		else
			_flushFrame = _continuousOutput;
	}
	else
	{
		_flushFrame = true;

		if (correction)
			setupAdvColor(deltaTime, kOrg, kMin, kMid, kAbove, kMax);
		else
			k = std::max((1 << 8) - (deltaTime << 8) / (_targetTime - _previousTime), static_cast<int64_t>(1));

		int redDiff = 0, greenDiff = 0, blueDiff = 0;

		if (_previousValues.size() != _targetValues.size())
		{
			Error(_log, "Detect abnormal state. Previuos value: %d, new value: %d", _previousValues.size(), _targetValues.size());
		}
		else
		{
			for (size_t i = 0; i < _previousValues.size(); i++)
			{
				ColorRgb& prev = _previousValues[i];
				ColorRgb& target = _targetValues[i];

				redDiff = target.red - prev.red;
				greenDiff = target.green - prev.green;
				blueDiff = target.blue - prev.blue;

				if (redDiff != 0)
					prev.red += (redDiff < 0 ? -1 : 1) *
					((correction) ? computeAdvColor(aspectLow, aspectMid, aspectHigh, kMin, kMid, kAbove, kMax, redDiff) : computeColor(k, redDiff));
				if (greenDiff != 0)
					prev.green += (greenDiff < 0 ? -1 : 1) *
					((correction) ? computeAdvColor(aspectLow, aspectMid, aspectHigh, kMin, kMid, kAbove, kMax, greenDiff) : computeColor(k, greenDiff));
				if (blueDiff != 0)
					prev.blue += (blueDiff < 0 ? -1 : 1) *
					((correction) ? computeAdvColor(aspectLow, aspectMid, aspectHigh, kMin, kMid, kAbove, kMax, blueDiff) : computeColor(k, blueDiff));
			}
		}
		_previousTime = now;

		queueColors(_previousValues);
	}
}

void LinearSmoothing::DebugOutput()
{
	/*_debugCounter = std::min(_debugCounter+1,900);
	if (_targetValues.size() > 0 && _debugCounter < 900)
	{
		if (_debugCounter < 100 || (_debugCounter > 200 && _debugCounter < 300) || (_debugCounter > 400 && _debugCounter < 500) || (_debugCounter > 600 && _debugCounter < 700) || _debugCounter>800)
		{
			_targetValues[0].red = 0;
			_targetValues[0].green = 0;
			_targetValues[0].blue = 0;
		}
		else if (_debugCounter >= 100 && _debugCounter <= 200)
		{
			_targetValues[0].red = 255;
			_targetValues[0].green = 255;
			_targetValues[0].blue = 255;
		}
		else if (_debugCounter >= 300 && _debugCounter <= 400)
		{
			_targetValues[0].red = 255;
			_targetValues[0].green = 0;
			_targetValues[0].blue = 0;
		}
		else if (_debugCounter >= 500 && _debugCounter <= 600)
		{
			_targetValues[0].red = 0;
			_targetValues[0].green = 255;
			_targetValues[0].blue = 0;
		}
		else if (_debugCounter >= 700 && _debugCounter <= 800)
		{
			_targetValues[0].red = 0;
			_targetValues[0].green = 0;
			_targetValues[0].blue = 255;
		}
	}*/
}


