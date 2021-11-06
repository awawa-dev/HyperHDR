// Qt includes
#include <QDateTime>
#include <QTimer>

#include <hyperhdrbase/LinearColorSmoothing.h>
#include <hyperhdrbase/HyperHdrInstance.h>

#include <cmath>
#include <stdint.h>
#include <inttypes.h>

using namespace hyperhdr;

const int64_t  DEFAUL_SETTLINGTIME    = 200;	// settlingtime in ms
const double   DEFAUL_UPDATEFREQUENCY = 25;	// updatefrequncy in hz
const int64_t  DEFAUL_UPDATEINTERVALL = static_cast<int64_t>(1000 / DEFAUL_UPDATEFREQUENCY); // updateintervall in ms


LinearColorSmoothing::LinearColorSmoothing(const QJsonDocument& config, HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr)
	, _log(Logger::getInstance(QString("SMOOTHING%1").arg(hyperhdr->getInstanceIndex())))
	, _semaphore(1)
	, _hyperhdr(hyperhdr)
	, _updateInterval(DEFAUL_UPDATEINTERVALL)
	, _settlingTime(DEFAUL_SETTLINGTIME)
	, _timer(this)
	, _continuousOutput(false)
	, _antiFlickeringTreshold(0)
	, _antiFlickeringStep(0)
	, _antiFlickeringTimeout(0)
	, _flushFrame(false)
	, _targetTime(0)
	, _previousTime(0)
	, _pause(false)
	, _currentConfigId(0)
	, _enabled(false)
	, _directMode(false)
	, _smoothingType(SmoothingType::Linear)
	, _infoUpdate(true)
	, _infoInput(true)
	, debugCounter(0)
{
	// init cfg 0 (default)
	addConfig(DEFAUL_SETTLINGTIME, DEFAUL_UPDATEFREQUENCY);
	handleSettingsUpdate(settings::type::SMOOTHING, config);
	selectConfig(0, true);

	// add pause on cfg 1
	SmoothingCfg cfg (true, 0, 0, false);
	_cfgList.append(cfg);

	// listen for comp changes
	connect(_hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &LinearColorSmoothing::componentStateChange);

	// timer
	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &LinearColorSmoothing::updateLeds);
}

inline uint8_t LinearColorSmoothing::clamp(int x)
{
	return (x < 0) ? 0 : ((x > 255) ? 255 : uint8_t(x));
}

void LinearColorSmoothing::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::type::SMOOTHING)
	{		
		QJsonObject obj = config.object();

		if (enabled() != obj["enable"].toBool(true))
		{
			setEnable(obj["enable"].toBool(true));
		}

		_continuousOutput = obj["continuousOutput"].toBool(true);

		SmoothingCfg	cfg (false,
							 static_cast<int64_t>(obj["time_ms"].toInt(DEFAUL_SETTLINGTIME)),
							 static_cast<int64_t>(std::max(1000.0/std::max(obj["updateFrequency"].toDouble(DEFAUL_UPDATEFREQUENCY), DEFAUL_UPDATEFREQUENCY), 5.0)),
							 false);

		auto smoothingType = obj["type"].toString("linear");
		if (smoothingType == "alternative") 
			cfg._type = SmoothingType::Alternative;		
		else
			cfg._type = SmoothingType::Linear;

		cfg._antiFlickeringTreshold = obj["lowLightAntiFlickeringTreshold"].toInt(0);
		cfg._antiFlickeringStep = obj["lowLightAntiFlickeringValue"].toInt(0);
		cfg._antiFlickeringTimeout = obj["lowLightAntiFlickeringTimeout"].toInt(0);

		Info( _log, "Creating config (%d) => type: %s, dirMode: %s, pause: %s, settlingTime: %ims, interval: %ims (%iHz), antiFlickTres: %i, antiFlickStep: %i, antiFlickTime: %i",
					_currentConfigId, QSTRING_CSTR(SmoothingCfg::EnumToString(cfg._type)), (cfg._directMode)?"true":"false", (cfg._pause)?"true":"false", int(cfg._settlingTime), int(cfg._updateInterval), int(1000.0/cfg._updateInterval), cfg._antiFlickeringTreshold, cfg._antiFlickeringStep, int(cfg._antiFlickeringTimeout));
		_cfgList[0] = cfg;

		// if current id is 0, we need to apply the settings (forced)
		if( _currentConfigId == 0)
		{		
			selectConfig(0, true);
		}
	}
}

int LinearColorSmoothing::write(const std::vector<ColorRgb> &ledValues)
{
	if (_directMode)
	{
		if (_timer.remainingTime() >= 0)
			clearQueuedColors(true);

		if (_pause || ledValues.size() == 0)
			return 0;

		emit _hyperhdr->ledDeviceData(ledValues);
			return 0;
	}

	try
	{
		_semaphore.acquire();		

		if (_smoothingType == SmoothingType::Alternative)
		{
			if (_infoInput)
				Info(_log, "Using alternative smoothing input (%i)", _currentConfigId);
			_infoInput = false;

			AlternateSetup(ledValues);
		}
		else {
			if (_infoInput)
				Info(_log, "Using linear smoothing input (%i)", _currentConfigId);
			_infoInput = false;

			LinearSetup(ledValues);
		}		

		if (!_timer.isActive() && _timer.remainingTime() < 0)
		{
			_timer.start(_updateInterval);

			Info(_log, "Enabling timer. Now timer is active: %i, remaining time to run: %i", _timer.isActive(), _timer.remainingTime());
		}
		else if ((QDateTime::currentMSecsSinceEpoch() - _previousTime) > qMax(10 * _updateInterval, (int64_t)2000))
		{
			_previousTime = QDateTime::currentMSecsSinceEpoch();
			_previousValues = ledValues;
			_previousTimeouts.clear();
			_previousTimeouts.resize(_previousValues.size(), _previousTime);

			_timer.start(_updateInterval);

			Warning(_log, "Emergency timer restart. Now timer is active: %i, remaining time to run: %i", _timer.isActive(), _timer.remainingTime());
		}

		_semaphore.release();
	}
	catch (...)
	{
		_semaphore.release();
		throw;
	}

	return 0;
}


int LinearColorSmoothing::updateLedValues(const std::vector<ColorRgb>& ledValues)
{
	int retval = 0;

	if (!_enabled)
	{
		return -1;
	}
	else
	{		
		retval = write(ledValues);
	}

	return retval;
}


void LinearColorSmoothing::Antiflickering()
{
	if (_antiFlickeringTreshold > 0 && _antiFlickeringStep > 0 && _previousValues.size() == _targetValues.size() && _previousValues.size() == _previousTimeouts.size())
	{
		int64_t now = QDateTime::currentMSecsSinceEpoch();

		for (size_t i = 0; i < _previousValues.size(); ++i)
		{
			auto	 newColor = _targetValues[i];
			auto     oldColor = _previousValues[i];
			int64_t& timeout  = _previousTimeouts[i];

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
					if (_antiFlickeringTimeout <= 0 || now - timeout < _antiFlickeringTimeout)
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

void LinearColorSmoothing::AlternateSetup(const std::vector<ColorRgb>& ledValues)
{
	LinearSetup(ledValues);
}

inline int Ceil(int limitMin, int limitAverage, int limitMax, float kMin, float kMid, float kAbove, float kMax, int color)
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

void LinearColorSmoothing::AlternateLinearSmoothing()
{
	int64_t now = QDateTime::currentMSecsSinceEpoch();
	int64_t deltaTime = _targetTime - now;

	if (deltaTime <= 0 || _targetTime <= _previousTime)
	{
		_previousValues = _targetValues;
		_previousTimeouts.clear();
		_previousTimeouts.resize(_previousValues.size(), now);

		_previousTime = now;

		if (_flushFrame)
			queueColors(_previousValues);

		_flushFrame = _continuousOutput;
	}
	else
	{
		_flushFrame = true;

		float kOrg = std::max( 1.0f - 1.0f * deltaTime / (_targetTime - _previousTime) , 0.0001f);

		float kMin = std::pow(kOrg, 1.0f);
		float kMid = std::pow(kOrg, 0.9f);
		float kAbove = std::pow(kOrg, 0.75f);
		float kMax = std::pow(kOrg, 0.6f);

		int redDiff = 0, greenDiff = 0, blueDiff = 0;
		int aspectLow = 16;
		int aspectMid = 32;
		int aspectHigh = 60;

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
					prev.red += (redDiff < 0 ? -1 : 1) * Ceil(aspectLow, aspectMid, aspectHigh, kMin, kMid, kAbove, kMax, redDiff);
				if (greenDiff != 0)
					prev.green += (greenDiff < 0 ? -1 : 1) * Ceil(aspectLow, aspectMid, aspectHigh, kMin, kMid, kAbove, kMax, greenDiff);
				if (blueDiff != 0)
					prev.blue += (blueDiff < 0 ? -1 : 1) * Ceil(aspectLow, aspectMid, aspectHigh, kMin, kMid, kAbove, kMax, blueDiff);
			}
		}
		_previousTime = now;

		queueColors(_previousValues);
	}
}


void LinearColorSmoothing::LinearSetup(const std::vector<ColorRgb>& ledValues)
{
	_targetTime = QDateTime::currentMSecsSinceEpoch() + _settlingTime;
	_targetValues = ledValues;
	
	/////////////////////////////////////////////////////////////////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!	

	if (!_previousValues.empty() && (_previousValues.size() != _targetValues.size()))
	{
		Warning(_log, "Detect size changed. Previuos value: %d, new value: %d", _previousValues.size(), _targetValues.size());
		_previousValues.clear();
		_previousTimeouts.clear();
	}

	if (_previousValues.empty())
	{
		_previousTime = QDateTime::currentMSecsSinceEpoch();
		_previousValues = ledValues;
		_previousTimeouts.clear();
		_previousTimeouts.resize(_previousValues.size(), _previousTime);
	}

	Antiflickering();
}

void LinearColorSmoothing::LinearSmoothing()
{
	int64_t now = QDateTime::currentMSecsSinceEpoch();
	int64_t deltaTime = _targetTime - now;

	if (deltaTime <= 0 || _targetTime <= _previousTime)
	{
		_previousValues = _targetValues;
		_previousTimeouts.clear();
		_previousTimeouts.resize(_previousValues.size(), now);

		_previousTime = now;
		
		if (_flushFrame)
			queueColors(_previousValues);

		_flushFrame = _continuousOutput;
	}
	else
	{
		_flushFrame = true;

		float k = std::max(1.0f - 1.0f * deltaTime / (_targetTime - _previousTime), 0.0001f);

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
					prev.red += (redDiff < 0 ? -1 : 1) * std::ceil(k * std::abs(redDiff));
				if (greenDiff != 0)
					prev.green += (greenDiff < 0 ? -1 : 1) * std::ceil(k * std::abs(greenDiff));
				if (blueDiff != 0)
					prev.blue += (blueDiff < 0 ? -1 : 1) * std::ceil(k * std::abs(blueDiff));
			}
		}
		_previousTime = now;

		queueColors(_previousValues);
	}	
}

void LinearColorSmoothing::updateLeds()
{	
	try
	{
		_semaphore.acquire();

		if (_smoothingType == SmoothingType::Alternative)
		{
			if (_infoUpdate)
				Info(_log, "Using alternative smoothing procedure (%i)", _currentConfigId);
			_infoUpdate = false;

			AlternateLinearSmoothing();
		}
		else
		{
			if (_infoUpdate)
				Info(_log, "Using linear smoothing procedure (%i)", _currentConfigId);
			_infoUpdate = false;

			LinearSmoothing();
		}

		_semaphore.release();
	}
	catch (...)
	{
		_semaphore.release();
		throw;
	}
}

void LinearColorSmoothing::queueColors(const std::vector<ColorRgb> & ledColors)
{	
	if (!_pause)
	{
		emit _hyperhdr->ledDeviceData(ledColors);
	}	
}

void LinearColorSmoothing::clearQueuedColors(bool semaphore)
{
	try
	{
		if (semaphore)
			_semaphore.acquire();

		Info(_log, "Clearing queued colors");

		_timer.stop();
		_previousValues.clear();
		_previousTimeouts.clear();
		_previousTime = 0;
		_targetValues.clear();
		_targetTime = 0;
		_flushFrame = false;
		_infoUpdate = true;
		_infoInput = true;

		if (semaphore)
			_semaphore.release();
	}
	catch (...)
	{
		if (semaphore)
			_semaphore.release();

		throw;
	}
}

void LinearColorSmoothing::componentStateChange(hyperhdr::Components component, bool state)
{
	_flushFrame = state;

	if(component == hyperhdr::COMP_LEDDEVICE)
	{
		clearQueuedColors(true);
	}

	if(component == hyperhdr::COMP_SMOOTHING)
	{
		setEnable(state);
	}
}

void LinearColorSmoothing::setEnable(bool enable)
{
	_enabled = enable;

	if (!_enabled)
	{
		clearQueuedColors(true);
	}

	// update comp register
	_hyperhdr->setNewComponentState(hyperhdr::COMP_SMOOTHING, enable);
}

void LinearColorSmoothing::setPause(bool pause)
{
	_pause = pause;
}

unsigned LinearColorSmoothing::addConfig(int settlingTime_ms, double ledUpdateFrequency_hz, bool directMode)
{
	SmoothingCfg cfg (false, settlingTime_ms, int64_t(1000.0/ledUpdateFrequency_hz), directMode);
	_cfgList.append(cfg);

	return _cfgList.count() - 1;
}

unsigned LinearColorSmoothing::updateConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool directMode)
{
	unsigned updatedCfgID = cfgID;
	if ( cfgID < static_cast<unsigned>(_cfgList.count()) )
	{
		SmoothingCfg cfg (false, settlingTime_ms, int64_t(1000.0/ledUpdateFrequency_hz), directMode);
		_cfgList[updatedCfgID] = cfg;
	}
	else
	{
		updatedCfgID = addConfig ( settlingTime_ms, ledUpdateFrequency_hz, directMode);
	}

	return updatedCfgID;
}

bool LinearColorSmoothing::selectConfig(unsigned cfg, bool force)
{
	if (_currentConfigId == cfg && !force)
	{
		return true;
	}

	if ( cfg < (unsigned)_cfgList.count())
	{
		_settlingTime     = _cfgList[cfg]._settlingTime;
		_pause            = _cfgList[cfg]._pause;
		_directMode       = _cfgList[cfg]._directMode;		
		_antiFlickeringTreshold = _cfgList[cfg]._antiFlickeringTreshold;
		_antiFlickeringStep     = _cfgList[cfg]._antiFlickeringStep;
		_antiFlickeringTimeout  = _cfgList[cfg]._antiFlickeringTimeout;

		int64_t newUpdateInterval = std::max(_cfgList[cfg]._updateInterval, (int64_t)5);

		if (newUpdateInterval != _updateInterval || _cfgList[cfg]._type  != _smoothingType)
		{
			clearQueuedColors(true);

			_updateInterval = newUpdateInterval;
			_smoothingType = _cfgList[cfg]._type;
		}

		_currentConfigId = cfg;

		Info( _log, "Selecting config (%d) => type: %s, dirMode: %s, pause: %s, settlingTime: %ims, interval: %ims (%iHz), antiFlickTres: %i, antiFlickStep: %i, antiFlickTime: %i",
					_currentConfigId, QSTRING_CSTR(SmoothingCfg::EnumToString(_smoothingType)), (_directMode)?"true":"false" , (_pause)?"true":"false", int(_settlingTime),
					int(_updateInterval), int(1000.0/ _updateInterval), _antiFlickeringTreshold, _antiFlickeringStep, int(_antiFlickeringTimeout));

		return true;
	}

	// reset to default
	_currentConfigId = 0;
	return false;
}

void LinearColorSmoothing::DebugOutput()
{
	/*debugCounter = std::min(debugCounter+1,900);
	if (_targetValues.size() > 0 && debugCounter < 900)
	{
		if (debugCounter < 100 || (debugCounter > 200 && debugCounter < 300) || (debugCounter > 400 && debugCounter < 500) || (debugCounter > 600 && debugCounter < 700) || debugCounter>800)
		{
			_targetValues[0].red = 0;
			_targetValues[0].green = 0;
			_targetValues[0].blue = 0;
		}
		else if (debugCounter >= 100 && debugCounter <= 200)
		{
			_targetValues[0].red = 255;
			_targetValues[0].green = 255;
			_targetValues[0].blue = 255;
		}
		else if (debugCounter >= 300 && debugCounter <= 400)
		{
			_targetValues[0].red = 255;
			_targetValues[0].green = 0;
			_targetValues[0].blue = 0;
		}
		else if (debugCounter >= 500 && debugCounter <= 600)
		{
			_targetValues[0].red = 0;
			_targetValues[0].green = 255;
			_targetValues[0].blue = 0;
		}
		else if (debugCounter >= 700 && debugCounter <= 800)
		{
			_targetValues[0].red = 0;
			_targetValues[0].green = 0;
			_targetValues[0].blue = 255;
		}
	}*/
}
