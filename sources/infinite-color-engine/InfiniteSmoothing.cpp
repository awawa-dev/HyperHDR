/* InfiniteSmoothing.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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

	#include <algorithm>
	#include <chrono>
	#include <iomanip>
	#include <iostream>
	#include <limits>
	#include <tuple>
	#include <vector>
	#include <cassert>
	#include <cmath>
	#include <cstdint>
#endif

#include <QMutexLocker>

#include <infinite-color-engine/InfiniteSmoothing.h>
#include <infinite-color-engine/InfiniteStepperInterpolator.h>
#include <infinite-color-engine/InfiniteYuvInterpolator.h>
#include <infinite-color-engine/InfiniteRgbInterpolator.h>
#include <infinite-color-engine/InfiniteHybridInterpolator.h>
#include <infinite-color-engine/InfiniteExponentialInterpolator.h>
#include <base/HyperHdrInstance.h>

using namespace hyperhdr;
using namespace linalg::aliases;

namespace
{
	constexpr auto SMOOTHING_COOLDOWN_PHASE = 3;
	constexpr auto SMOOTHING_USER_CONFIG = 0;
	constexpr int64_t  DEFAUL_SETTLINGTIME = 200;   // settlingtime in ms
	constexpr double   DEFAUL_UPDATEFREQUENCY = 25;    // updatefrequncy in hz
	constexpr double   MINIMAL_UPDATEFREQUENCY = 20;

	struct TestPilot
	{
		bool active = false;
		long long started = 0;
		std::vector<float4> sequence = {
			{0,    0.0f, 0.0f, 0.0f},  // start from black color
			{2000, 1.0f, 0.0f, 0.0f},  // short (100ms) red color blink at 2.0sec
			{2100, 0.0f, 0.0f, 0.0f},  // return to black color
			{2300, 0.0f, 1.0f, 0.0f},  // short (100ms) green color blink at 2.3sec
			{2400, 0.0f, 0.0f, 0.0f},  // return to black color
			{2600, 0.0f, 0.0f, 1.0f},  // short (100ms) blue color blink at 2.6sec
			{2700, 0.0f, 0.0f, 0.0f},  // return to black color
			{3000, 0.0f, 0.0f, 1.0f},  // start moving to blue color at 3sec
			{3200, 0.0f, 1.0f, 0.0f},  // then turn to green color at 3.2sec
			{3400, 1.0f, 0.0f, 0.0f},  // then immediately turn to red color at 3.4sec and wait
			{4000, 0.0f, 0.0f, 1.0f},  // start moving to blue color at 4sec
			{4100, 1.0f, 0.0f, 0.0f},  // then turn to red color at 4.1sec
			{4200, 0.0f, 1.0f, 0.0f},  // the turn to green at 4.2sec and wait 
			{6000, 0.0f, 0.0f, 0.0f},  // finally return to black color at 6sec
			{8000, 0.0f, 0.0f, 0.0f}   // the end of the cycle at 6sec
		};
		float3 getColorAtTime(long long currentTimeMs)
		{
			long long delta = currentTimeMs - started;

			if (!sequence.empty() && delta > static_cast<long long>(sequence.back().x)) {
				started = currentTimeMs;
				delta = 0;
			}

			float3 color = { 0.0f, 0.0f, 0.0f };

			for (size_t i = 1; i < sequence.size(); ++i) {
				if (delta < static_cast<long long>(sequence[i].x)) {
					color = { sequence[i - 1].y, sequence[i - 1].z, sequence[i - 1].w };
					break;
				}
			}

			printf("Δ%4lld | incomingColors: (%d, %d, %d)", delta, (int)(color.x * 255), (int)(color.y * 255), (int)(color.z * 255));

			return color;
		}
	};

	TestPilot& getTestPilot()
	{
		static TestPilot testPilot;
		return testPilot;
	}
}

InfiniteSmoothing::InfiniteSmoothing(const QJsonDocument& config, HyperHdrInstance* hyperhdr)
	: QObject(),
	_log(QString("SMOOTHING%1").arg(hyperhdr->getInstanceIndex())),
	_hyperhdr(hyperhdr),
	_continuousOutput(false),
	_currentConfigId(SMOOTHING_USER_CONFIG),
	_enabled(false),
	_connected(false),
	_interpolator(std::make_unique<InfiniteStepperInterpolator>()),
	_infoUpdate(true),
	_infoInput(true),
	_coolDown(0)
{
	// init cfg 0 (SMOOTHING_USER_CONFIG)
	addConfig(DEFAUL_SETTLINGTIME, DEFAUL_UPDATEFREQUENCY);
	handleSignalInstanceSettingsChanged(settings::type::SMOOTHING, config);
	selectConfig(SMOOTHING_USER_CONFIG);
}

void InfiniteSmoothing::clearQueuedColors(bool deviceEnabled, bool restarting)
{
	// critical section
	QMutexLocker locker(&_dataSynchro);

	try
	{
		Info(_log, "Clearing queued colors before: {:s}{:s}",
			(deviceEnabled) ? "enabling" : "disabling",
			(restarting) ? ". Smoothing configuration changed: restarting timer." : "");

		if ((!deviceEnabled || restarting) && _connected)
		{
			_connected = false;
			disconnect(this, &InfiniteSmoothing::SignalMasterClockTick, this, &InfiniteSmoothing::updateLeds);
		}
		
		_infoUpdate = true;
		_infoInput = true;
		_coolDown = 0;

		if (restarting || _interpolator == nullptr)
		{
			const auto& cfg = _configurations[_currentConfigId];

			if (cfg->type == SmoothingType::HybridInterpolator)
			{
				_interpolator = std::make_unique<InfiniteHybridInterpolator>();
			}
			else if (cfg->type == SmoothingType::YuvInterpolator)
			{
				_interpolator = std::make_unique<InfiniteYuvInterpolator>();
			}
			else if (cfg->type == SmoothingType::RgbInterpolator)
			{
				_interpolator = std::make_unique<InfiniteRgbInterpolator>();
			}
			else if (cfg->type == SmoothingType::ExponentialInterpolator)
			{
				_interpolator = std::make_unique<InfiniteExponentialInterpolator>();
			}
			else
			{
				_interpolator = std::make_unique<InfiniteStepperInterpolator>();
			}

			_interpolator->setTransitionDuration(cfg->settlingTime);
			_interpolator->setSmoothingFactor(cfg->smoothingFactor);
			_interpolator->setSpringiness(cfg->stiffness, cfg->damping);
			_interpolator->setMaxLuminanceChangePerFrame(cfg->y_limit);
		}

		if (deviceEnabled && !_connected)
		{
			_connected = true;
			connect(this, &InfiniteSmoothing::SignalMasterClockTick, this, &InfiniteSmoothing::updateLeds, Qt::DirectConnection);
		}

		emit _hyperhdr->SignalSmoothingRestarted(this->getSuggestedInterval());

		Info(_log, "Smoothing queue is cleared");
	}
	catch (...)
	{
		Debug(_log, "Smoothing error detected");
	}
}

void InfiniteSmoothing::handleSignalInstanceSettingsChanged(settings::type type, const QJsonDocument& config)
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
			setEnable(obj["enable"].toBool(true));
		}

		_continuousOutput = obj["continuousOutput"].toBool(true);

		if (getTestPilot().active = obj["testMode"].toBool(false); getTestPilot().active)
		{
			getTestPilot().started = InternalClock::now();
		}

		_configurations[SMOOTHING_USER_CONFIG] = std::make_unique<SmoothingConfig>(
			SmoothingConfig{
			.pause = false,
			.settlingTime = static_cast<int64_t>(obj["time_ms"].toInt(DEFAUL_SETTLINGTIME)),
			.updateInterval = static_cast<int64_t>(std::round(std::max(1000.0 / std::max(obj["updateFrequency"].toDouble(DEFAUL_UPDATEFREQUENCY), MINIMAL_UPDATEFREQUENCY), 5.0))),
			.type = StringToEnumSmoothingType(obj["type"].toString()),
			.smoothingFactor = static_cast<float>(obj["smoothingFactor"].toDouble(0.1)),
			.stiffness = static_cast<float>(obj["stiffness"].toDouble(200)),
			.damping = static_cast<float>(obj["damping"].toDouble(26)),
			.y_limit = static_cast<float>(obj["y_limit"].toDouble())
			}
		);

		const auto& cfg = _configurations[SMOOTHING_USER_CONFIG];
		Info(_log, "Updating user config ({:d}) => type: {:s}, pause: {:s}, settlingTime: {:d}ms, interval: {:d}ms ({:d}Hz)"
			       ", smoothingFactor: {:f}, stiffness: {:f}, damping: {:f}, y_limit: {:f}",
					SMOOTHING_USER_CONFIG, (EnumSmoothingTypeToString(cfg->type)), (cfg->pause) ? "true" : "false", int(cfg->settlingTime), int(cfg->updateInterval), int(1000.0 / cfg->updateInterval),
					cfg->smoothingFactor, cfg->stiffness, cfg->damping, cfg->y_limit
			);

		if (_currentConfigId == SMOOTHING_USER_CONFIG)
		{
			if (isEnabled())
				QUEUE_CALL_0(_hyperhdr, update);
			selectConfig(SMOOTHING_USER_CONFIG);
		}
	}
}

void InfiniteSmoothing::incomingColors(std::vector<float3>&& nonlinearRgbColors)
{
	if (_infoInput)
	{
		if (!isEnabled())
			Info(_log, "Smoothing is disabled. Direct output.");
		else 
			Info(_log, "Using {:s} smoothing input ({:d})", (EnumSmoothingTypeToString(_configurations[_currentConfigId]->type)), _currentConfigId);
		_infoInput = false;
	}


	if (!isEnabled())
	{
		queueColors(std::make_shared<std::vector<float3>>(std::move(nonlinearRgbColors)));
		return;
	}	

	// critical section
	{
		QMutexLocker locker(&_dataSynchro);

		auto nowTime = InternalClock::now();
		if (getTestPilot().active)
		{
			std::fill(nonlinearRgbColors.begin(), nonlinearRgbColors.end(), getTestPilot().getColorAtTime(nowTime));
		}
		_interpolator->setTargetColors(std::move(nonlinearRgbColors), nowTime, getTestPilot().active);
	}
}

void InfiniteSmoothing::updateLeds()
{	
	SharedOutputColors nonlinearRgbColors;
	bool finished = false;
	// critical section
	{
		QMutexLocker locker(&_dataSynchro);
		if (!isEnabled())
			return;
		_interpolator->updateCurrentColors(InternalClock::now());

		nonlinearRgbColors = _interpolator->getCurrentColors();

		if (!_interpolator->isAnimationComplete())
		{
			_coolDown = SMOOTHING_COOLDOWN_PHASE;
		}
		else
		{
			finished = (!_continuousOutput) && (_coolDown <= 0 || _coolDown-- <= 0);
		}
	}	

	if (!nonlinearRgbColors->empty() && !finished)
	{
		queueColors(std::move(nonlinearRgbColors));
	}
}

void InfiniteSmoothing::queueColors(SharedOutputColors&& nonlinearRgbLedColors)
{
	if (nonlinearRgbLedColors->empty())
		return;

	if (getTestPilot().active)
	{
		const auto& color = nonlinearRgbLedColors->front();
		long long delta = InternalClock::now() - getTestPilot().started;
		printf("Δ%4lld | updateCurrentColors: (%d, %d, %d)\n", delta, (int)(color.x * 255), (int)(color.y * 255), (int)(color.z * 255));
	}

	emit SignalProcessedColors(nonlinearRgbLedColors);
}

void InfiniteSmoothing::handleSignalRequestComponent(hyperhdr::Components component, bool state)
{
	if (component == hyperhdr::COMP_LEDDEVICE)
	{
		clearQueuedColors(state);
	}

	if (component == hyperhdr::COMP_SMOOTHING)
	{
		setEnable(state);
	}
}

void InfiniteSmoothing::setEnable(bool enable)
{
	_enabled = enable;

	clearQueuedColors(isEnabled());

	_hyperhdr->setNewComponentState(hyperhdr::COMP_SMOOTHING, enable);
}

unsigned InfiniteSmoothing::addConfig(int settlingTime_ms, double ledUpdateFrequency_hz, bool pause)
{
	_configurations.push_back(std::make_unique<SmoothingConfig>(
		SmoothingConfig{ .pause = pause, .settlingTime = settlingTime_ms, .updateInterval = int64_t(1000.0 / ledUpdateFrequency_hz) }));

	return static_cast<unsigned>(_configurations.size() - 1);
}

unsigned InfiniteSmoothing::addCustomSmoothingConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool pause)
{
	int64_t interval =  (ledUpdateFrequency_hz > std::numeric_limits<double>::epsilon()) ? static_cast<int64_t>(1000.0 / ledUpdateFrequency_hz) : 10;
	
	if (cfgID < static_cast<unsigned>(_configurations.size()))
	{
		_configurations[cfgID] = std::make_unique<SmoothingConfig>(SmoothingConfig{ .pause = pause, .settlingTime = settlingTime_ms, .updateInterval = interval, .type = SmoothingType::Stepper });
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

void InfiniteSmoothing::setCurrentSmoothingConfigParams(unsigned cfgID)
{
	if (cfgID != _currentConfigId)
	{
		selectConfig(cfgID);
	}
}

int InfiniteSmoothing::getSuggestedInterval()
{
	return (isEnabled()) ? static_cast<int>(_configurations[_currentConfigId]->updateInterval) : 0;
}

bool InfiniteSmoothing::selectConfig(unsigned cfgId)
{
	bool result = (cfgId < (unsigned)_configurations.size());

	_currentConfigId = (result) ? cfgId : SMOOTHING_USER_CONFIG;

	clearQueuedColors(isEnabled(), true);

	const auto& cfg = _configurations[_currentConfigId];

	Info(_log, "Selecting config ({:d}) => type: {:s}, pause: {:s}, settlingTime: {:d}ms, interval: {:d}ms ({:d}Hz). Smoothing is currently: {:s}"
				", smoothingFactor: {:f}, stiffness: {:f}, damping: {:f}, y_limit: {:f}",
		_currentConfigId, (EnumSmoothingTypeToString(cfg->type)), (!cfg->pause) ? "true" : "false",
		int(cfg->settlingTime),
		int(cfg->updateInterval),
		int(1000.0 / cfg->updateInterval),
		(_enabled) ? "enabled" : "disabled",
		cfg->smoothingFactor, cfg->stiffness, cfg->damping, cfg->y_limit);

	return result;
}

bool InfiniteSmoothing::isEnabled() const
{
	return _enabled && !_configurations[_currentConfigId]->pause;
}

QString InfiniteSmoothing::EnumSmoothingTypeToString(SmoothingType type)
{
	if (type == SmoothingType::RgbInterpolator)
		return QString("RgbInterpolator");
	else if (type == SmoothingType::YuvInterpolator)
		return QString("YuvInterpolator");
	else if (type == SmoothingType::HybridInterpolator)
		return QString("HybridInterpolator");
	else if (type == SmoothingType::ExponentialInterpolator)
		return QString("ExponentialInterpolator");

	return QString("Stepper");
}

InfiniteSmoothing::SmoothingType InfiniteSmoothing::StringToEnumSmoothingType(QString name)
{
	if (name == QString("RgbInterpolator"))
		return SmoothingType::RgbInterpolator;
	else if (name == QString("YuvInterpolator"))
		return SmoothingType::YuvInterpolator;
	else if (name == QString("HybridInterpolator"))
		return SmoothingType::HybridInterpolator;
	else if (name == QString("ExponentialInterpolator"))
		return SmoothingType::ExponentialInterpolator;

	return SmoothingType::Stepper;
}



