/* Smoothing.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#ifndef PCH_ENABLED
	#include <QTimer>
	#include <QThread>	

	#include <cmath>
	#include <stdint.h>
	#include <inttypes.h>
#endif

#include <limits>
#include <QMutexLocker>

#include <base/Smoothing.h>
#include <base/HyperHdrInstance.h>

using namespace hyperhdr;

namespace {
	const int64_t  DEFAUL_SETTLINGTIME = 200;   // settlingtime in ms
	const double   DEFAUL_UPDATEFREQUENCY = 25;    // updatefrequncy in hz
	const double   MINIMAL_UPDATEFREQUENCY = 20;
}

Smoothing::Smoothing(const QJsonDocument& config, HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr),
	_log(Logger::getInstance(QString("SMOOTHING%1").arg(hyperhdr->getInstanceIndex()))),
	_hyperhdr(hyperhdr),
	_updateInterval(static_cast<int64_t>(1000 / DEFAUL_UPDATEFREQUENCY)),
	_settlingTime(DEFAUL_SETTLINGTIME),
	_continuousOutput(false),
	_antiFlickeringTreshold(0),
	_antiFlickeringStep(0),
	_antiFlickeringTimeout(0),
	_flushFrame(false),
	_targetTimeUnsafe(0),
	_targetTimeCopy(0),
	_previousTime(0),
	_pause(false),
	_currentConfigId(0),
	_enabled(false),
	_hasData(false),
	_connected(false),
	_smoothingType(SmoothingType::Linear),
	_infoUpdate(true),
	_infoInput(true),
	_coolDown(0)
{
	// init cfg 0 (SMOOTHING_USER_CONFIG)
	addConfig(DEFAUL_SETTLINGTIME, DEFAUL_UPDATEFREQUENCY);
	handleSettingsUpdate(settings::type::SMOOTHING, config);
	SelectConfig(SMOOTHING_USER_CONFIG, true);

	// listen for comp changes
	connect(_hyperhdr, &HyperHdrInstance::SignalRequestComponent, this, &Smoothing::componentStateChange);
}

inline uint8_t Smoothing::clamp(int x)
{
	return (x < 0) ? 0 : ((x > 255) ? 255 : uint8_t(x));
}

void Smoothing::clearQueuedColors(bool deviceEnabled, bool restarting)
{
	QMutexLocker locker(&_setupSynchro);

	try
	{
		Info(_log, "Clearing queued colors before: %s%s",
			(deviceEnabled) ? "enabling" : "disabling",
			(restarting) ? ". Smoothing configuration changed: restarting timer." : "");

		if ((!deviceEnabled || restarting || _pause) && _connected)
		{
			_connected = false;
			disconnect(this, &Smoothing::SignalMasterClockTick, this, &Smoothing::updateLeds);
		}
		
		_currentColors.clear();
		_currentTimeouts.clear();
		_previousTime = 0;
		_targetColorsUnsafe.clear();
		_targetColorsCopy.clear();
		_targetTimeUnsafe = 0;
		_targetTimeCopy = 0;
		_flushFrame = false;
		_infoUpdate = true;
		_infoInput = true;
		_coolDown = 0;

		if (deviceEnabled && !_pause && !_connected)
		{
			_connected = true;
			connect(this, &Smoothing::SignalMasterClockTick, this, &Smoothing::updateLeds, Qt::DirectConnection);
		}

		emit _hyperhdr->SignalSmoothingRestarted(this->GetSuggestedInterval());

		Info(_log, "Smoothing queue is cleared");
	}
	catch (...)
	{
		Debug(_log, "Smoothing error detected");
	}
}

void Smoothing::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::SMOOTHING)
	{
		if (InternalClock::isPreciseSteady())
			Info(_log, "High resolution clock is steady (good)");
		else
			Warning(_log, "High resolution clock is NOT STEADY!");

		QJsonObject obj = config.object();

		if (isEnabled() != obj["enable"].toBool(true))
		{
			SetEnable(obj["enable"].toBool(true));
		}

		_continuousOutput = obj["continuousOutput"].toBool(true);

        _configurations[SMOOTHING_USER_CONFIG] = std::unique_ptr<SmoothingConfig>(new SmoothingConfig(
            false,
			static_cast<int64_t>(obj["time_ms"].toInt(DEFAUL_SETTLINGTIME)),
			static_cast<int64_t>(std::round(std::max(1000.0 / std::max(obj["updateFrequency"].toDouble(DEFAUL_UPDATEFREQUENCY), MINIMAL_UPDATEFREQUENCY), 5.0)))));

        auto& cfg = _configurations[SMOOTHING_USER_CONFIG];

		if (obj["type"].toString("linear") == "alternative")
			cfg->type = SmoothingType::Alternative;
		else
			cfg->type = SmoothingType::Linear;

		cfg->antiFlickeringTreshold = obj["lowLightAntiFlickeringTreshold"].toInt(0);
		cfg->antiFlickeringStep = obj["lowLightAntiFlickeringValue"].toInt(0);
		cfg->antiFlickeringTimeout = obj["lowLightAntiFlickeringTimeout"].toInt(0);

		Info(_log, "Creating config (%d) => type: %s, pause: %s, settlingTime: %ims, interval: %ims (%iHz), antiFlickTres: %i, antiFlickStep: %i, antiFlickTime: %i",
			_currentConfigId, QSTRING_CSTR(SmoothingConfig::EnumToString(cfg->type)), (cfg->pause) ? "true" : "false", int(cfg->settlingTime), int(cfg->updateInterval), int(1000.0 / cfg->updateInterval), cfg->antiFlickeringTreshold, cfg->antiFlickeringStep, int(cfg->antiFlickeringTimeout));

		// if current id is SMOOTHING_USER_CONFIG, we need to apply the settings (forced)
		if (_currentConfigId == SMOOTHING_USER_CONFIG)
		{
			if (_currentColors.size() > 0 && isEnabled())
				QUEUE_CALL_0(_hyperhdr, update);
			SelectConfig(SMOOTHING_USER_CONFIG, true);
		}
	}
}

void Smoothing::antiflickering(std::vector<ColorRgb>& newTarget)
{
	int64_t now = InternalClock::nowPrecise();

	if (_antiFlickeringTreshold > 0 && _antiFlickeringStep > 0)
	{
		for (size_t i = 0; i < _targetColorsUnsafe.size(); ++i)
		{
			ColorRgb& newColor = newTarget[i];
			const ColorRgb& oldColor = _targetColorsUnsafe[i];
			int64_t& timeout = _currentTimeouts[i];
			bool setNewColor = true;
			
			int averageNewColor =
						(static_cast<int>(std::min(newColor.red, std::min(newColor.green, newColor.blue))) +
						 static_cast<int>(std::max(newColor.red, std::max(newColor.green, newColor.blue)))) / 2;

			if (_antiFlickeringTimeout > 0 && averageNewColor < _antiFlickeringTreshold &&
				averageNewColor && oldColor.hasColor())
			{
				int minR = std::abs(int(newColor.red) - int(oldColor.red));
				int minG = std::abs(int(newColor.green) - int(oldColor.green));
				int minB = std::abs(int(newColor.blue) - int(oldColor.blue));
				int select = std::max(std::max(minR, minG), minB);

				if (select > 0 && select < _antiFlickeringStep)
				{
					if (now > timeout && now - timeout < _antiFlickeringTimeout)
					{
						setNewColor = false;
					}
				}
			}

			if (!setNewColor)
				newColor = oldColor;
			else
				timeout = now;			
		}
	}
}

void Smoothing::UpdateLedValues(const std::vector<ColorRgb>& ledValues)
{
	if (!_enabled)
		return;

	_coolDown = 1;

	if (_pause)
	{		
		if (_connected)
			clearQueuedColors();

		if (ledValues.size() == 0)
			return;

		queueColors(ledValues);

		return;
	}
	
	try
	{
		if (_infoInput)
			Info(_log, "Using %s smoothing input (%i)", ((_smoothingType == SmoothingType::Alternative) ? "alternative" : "linear"), _currentConfigId);

		_infoInput = false;
		linearSetup(ledValues);
	}
	catch (...)
	{
		Debug(_log, "Smoothing error detected");
	}

	return;
}

void Smoothing::linearSetup(const std::vector<ColorRgb>& ledValues)
{
	std::vector<ColorRgb> newTarget = ledValues;
	qint64 newTime = InternalClock::nowPrecise() + _settlingTime;

	if (newTarget.size() != _currentTimeouts.size())
	{
		_currentTimeouts = std::vector<int64_t>(newTarget.size(), InternalClock::nowPrecise());
	}
	else
	{
		antiflickering(newTarget);
	}

	_dataSynchro.lock();
	_hasData = true;
	_targetColorsUnsafe.swap(newTarget);
	_targetTimeUnsafe = newTime;	
	_dataSynchro.unlock();	
}

inline uint8_t Smoothing::computeColor(int64_t k, int color)
{
	int delta = std::abs(color);
	auto step = std::min((int)std::max(static_cast<int>((k * delta) >> 8), 1), delta);

	return step;
}

void Smoothing::setupAdvColor(int64_t deltaTime, float& kOrg, float& kMin, float& kMid, float& kAbove, float& kMax)
{
	kOrg = std::max(1.0f - 1.0f * deltaTime / (_targetTimeCopy - _previousTime), 0.0001f);

	kMin = std::min(std::pow(kOrg, 1.0f), 1.0f);
	kMid = std::min(std::pow(kOrg, 0.9f), 1.0f);
	kAbove = std::min(std::pow(kOrg, 0.75f), 1.0f);
	kMax = std::min(std::pow(kOrg, 0.6f), 1.0f);
}

inline uint8_t Smoothing::computeAdvColor(int limitMin, int limitAverage, int limitMax, float kMin, float kMid, float kAbove, float kMax, int color)
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

void Smoothing::updateLeds()
{
	if (!_enabled || !_setupSynchro.tryLock())
		return;

	try
	{
		if (_hasData && _dataSynchro.tryLock())
		{
			_hasData = false;
			_targetColorsCopy = _targetColorsUnsafe;
			_targetTimeCopy = _targetTimeUnsafe;
			_dataSynchro.unlock();
		}

		if (_currentColors.empty() || _currentColors.size() != _targetColorsCopy.size())
		{
			if (!_currentColors.empty())
				Warning(_log, "Detect buffer size has changed. Previous buffer size: %d, new size: %d.", _currentColors.size(), _targetColorsCopy.size());

			_previousTime = InternalClock::nowPrecise();
			_currentColors = _targetColorsCopy;			
		}		
		else if (_smoothingType == SmoothingType::Alternative)
		{
			if (_infoUpdate)
				Info(_log, "Using alternative smoothing procedure (%i)", _currentConfigId);
			_infoUpdate = false;

			linearSmoothingProcessing(true);
		}
		else
		{
			if (_infoUpdate)
				Info(_log, "Using linear smoothing procedure (%i)", _currentConfigId);
			_infoUpdate = false;

			linearSmoothingProcessing(false);
		}

		if (_hasData)
		{			
			_dataSynchro.lock();
			_hasData = false;
			_targetColorsCopy = _targetColorsUnsafe;
			_targetTimeCopy = _targetTimeUnsafe;
			_dataSynchro.unlock();
		}
	}
	catch (...)
	{
		Debug(_log, "Smoothing error detected");
	}

	_setupSynchro.unlock();
}

void Smoothing::queueColors(const std::vector<ColorRgb>& ledColors)
{
	emit _hyperhdr->SignalUpdateLeds(ledColors);
}

void Smoothing::componentStateChange(hyperhdr::Components component, bool state)
{
	_flushFrame = state;

	if (component == hyperhdr::COMP_LEDDEVICE)
	{
		clearQueuedColors(state);
	}

	if (component == hyperhdr::COMP_SMOOTHING)
	{
		SetEnable(state);
	}
}

void Smoothing::SetEnable(bool enable)
{
	_enabled = enable;

	clearQueuedColors(_enabled);

	// update comp register
	_hyperhdr->setNewComponentState(hyperhdr::COMP_SMOOTHING, enable);
}

unsigned Smoothing::addConfig(int settlingTime_ms, double ledUpdateFrequency_hz, bool pause)
{
	_configurations.push_back(std::unique_ptr<SmoothingConfig>(
        new SmoothingConfig(pause, settlingTime_ms, int64_t(1000.0 / ledUpdateFrequency_hz))));

	return static_cast<unsigned>(_configurations.size() - 1);
}

unsigned Smoothing::AddEffectConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool pause)
{
	int64_t interval =  (ledUpdateFrequency_hz > std::numeric_limits<double>::epsilon()) ? static_cast<int64_t>(1000.0 / ledUpdateFrequency_hz) : 10;
	
	if (cfgID < static_cast<unsigned>(_configurations.size()))
	{
        _configurations[cfgID] = std::unique_ptr<SmoothingConfig>(new SmoothingConfig(pause, settlingTime_ms, interval));
		return cfgID;
	}
	else
	{
		for (unsigned int currentCfgID = SMOOTHING_EFFECT_CONFIGS_START; currentCfgID < _configurations.size(); currentCfgID++)
		{
			auto& element = _configurations[currentCfgID];
			if ((element->settlingTime == settlingTime_ms &&
				element->updateInterval == interval) ||
				(pause && element->pause == pause))
			{
				return currentCfgID;
			}
		}
		return addConfig(settlingTime_ms, ledUpdateFrequency_hz, pause);
	}
}

void Smoothing::UpdateCurrentConfig(int settlingTime_ms)
{
	_settlingTime = settlingTime_ms;

	Info(_log, "Updating current config (%d) => type: %s, pause: %s, settlingTime: %ims, interval: %ims (%iHz), antiFlickTres: %i, antiFlickStep: %i, antiFlickTime: %i",
		_currentConfigId, QSTRING_CSTR(SmoothingConfig::EnumToString(_smoothingType)), (_pause) ? "true" : "false", int(_settlingTime),
		int(_updateInterval), int(1000.0 / _updateInterval), _antiFlickeringTreshold, _antiFlickeringStep, int(_antiFlickeringTimeout));
}

int Smoothing::GetSuggestedInterval()
{
	if (_enabled && !_pause)
	{
		return static_cast<int>(_updateInterval);
	}

	return 0;
}

bool Smoothing::SelectConfig(unsigned cfg, bool force)
{
	//WATCH
	if (_currentConfigId == cfg && !force)
	{
		return true;
	}

	if (cfg < (unsigned)_configurations.size())
	{
		_settlingTime = _configurations[cfg]->settlingTime;
		_pause = _configurations[cfg]->pause;
		_antiFlickeringTreshold = _configurations[cfg]->antiFlickeringTreshold;
		_antiFlickeringStep = _configurations[cfg]->antiFlickeringStep;
		_antiFlickeringTimeout = _configurations[cfg]->antiFlickeringTimeout;

		int64_t newUpdateInterval = std::max(_configurations[cfg]->updateInterval, (int64_t)5);

		if (newUpdateInterval != _updateInterval || _configurations[cfg]->type != _smoothingType)
		{
			_updateInterval = newUpdateInterval;
			_smoothingType = _configurations[cfg]->type;

			clearQueuedColors(!_pause && _enabled, true);
		}

		_currentConfigId = cfg;

		Info(_log, "Selecting config (%d) => type: %s, pause: %s, settlingTime: %ims, interval: %ims (%iHz), antiFlickTres: %i, antiFlickStep: %i, antiFlickTime: %i",
			_currentConfigId, QSTRING_CSTR(SmoothingConfig::EnumToString(_smoothingType)), (_pause) ? "true" : "false", int(_settlingTime),
			int(_updateInterval), int(1000.0 / _updateInterval), _antiFlickeringTreshold, _antiFlickeringStep, int(_antiFlickeringTimeout));

		return true;
	}

	// reset to default
	_currentConfigId = SMOOTHING_USER_CONFIG;
	return false;
}

bool Smoothing::isPaused() const
{
	return _pause;
}

bool Smoothing::isEnabled() const
{
	return _enabled && !_pause;
}

Smoothing::SmoothingConfig::SmoothingConfig(bool __pause, int64_t __settlingTime, int64_t __updateInterval,
            SmoothingType __type, int __antiFlickeringTreshold, int __antiFlickeringStep,
            int64_t __antiFlickeringTimeout) :
	pause(__pause),
	settlingTime(__settlingTime),
	updateInterval(__updateInterval),
	type(__type),
	antiFlickeringTreshold(__antiFlickeringTreshold),
	antiFlickeringStep(__antiFlickeringStep),
	antiFlickeringTimeout(__antiFlickeringTimeout)
{
}

QString Smoothing::SmoothingConfig::EnumToString(SmoothingType type)
{
	if (type == SmoothingType::Linear)
		return QString("Linear");
	else if (type == SmoothingType::Alternative)
		return QString("Alternative");

	return QString("Unknown");
}

void Smoothing::linearSmoothingProcessing(bool correction)
{
	float kOrg, kMin, kMid, kAbove, kMax;
	int64_t now = InternalClock::nowPrecise();
	int64_t deltaTime = _targetTimeCopy - now;
	int64_t k;

	int aspectLow = 16;
	int aspectMid = 32;
	int aspectHigh = 60;


	if (deltaTime <= 0 || _targetTimeCopy <= _previousTime)
	{
		_currentColors = _targetColorsCopy;	
		_previousTime = now;

		if (_flushFrame)
			queueColors(_currentColors);

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
			k = std::max((static_cast<int64_t>(1) << 8) - (deltaTime << 8) / (_targetTimeCopy - _previousTime), static_cast<int64_t>(1));

		
		for (size_t i = 0; i < _currentColors.size(); i++)
		{
			ColorRgb& prev = _currentColors[i];
			ColorRgb& target = _targetColorsCopy[i];

			int redDiff = target.red - prev.red;
			int greenDiff = target.green - prev.green;
			int blueDiff = target.blue - prev.blue;

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
		
		_previousTime = now;

		queueColors(_currentColors);
	}
}

void Smoothing::debugOutput(std::vector<ColorRgb>& _targetValues)
{
	static int _debugCounter = 0;
	_debugCounter = std::min(_debugCounter+1,900);
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
	}
}


