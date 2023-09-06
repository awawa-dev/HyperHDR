/* EffectEngine.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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

// Qt includes
#include <QResource>

#include <utils/jsonschema/QJsonSchemaChecker.h>
#include <utils/JsonUtils.h>
#include <utils/Components.h>

// effect engine includes
#include <effectengine/EffectEngine.h>
#include <effectengine/Effect.h>
#include <effectengine/EffectDBHandler.h>
#include "HyperhdrConfig.h"

EffectEngine::EffectEngine(HyperHdrInstance* hyperhdr)
	: _hyperInstance(hyperhdr)
	, _availableEffects()
	, _activeEffects()
	, _log(Logger::getInstance(QString("EFFECTENGINE%1").arg(hyperhdr->getInstanceIndex())))
	, _effectDBHandler(EffectDBHandler::getInstance())
{
	qRegisterMetaType<hyperhdr::Components>("hyperhdr::Components");

	// connect the HyperHDR channel clear feedback
	connect(_hyperInstance, &HyperHdrInstance::channelCleared, this, &EffectEngine::channelCleared);
	connect(_hyperInstance, &HyperHdrInstance::allChannelsCleared, this, &EffectEngine::allChannelsCleared);

	// get notifications about refreshed effect list
	connect(_effectDBHandler, &EffectDBHandler::effectListChanged, this, &EffectEngine::handleUpdatedEffectList);

	// register smooth cfgs and fill available effects
	handleUpdatedEffectList();
}

EffectEngine::~EffectEngine()
{
	auto copy = _activeEffects;

	_activeEffects.clear();

	for (Effect* effect : copy)
	{
		effect->requestInterruption();
		effect->quit();
	}

	for (Effect* effect : copy)
	{
		effect->wait();
		delete effect;
	}

	Debug(_log, "EffectEngine is released");
}

int EffectEngine::runEffectScript(const QString& name, const QJsonObject& args, int priority, int timeout, const QString& origin, unsigned smoothCfg, const QString& imageData)
{
	// clear current effect on the channel
	channelCleared(priority);

	// create the effect
	Effect* effect = new Effect(_hyperInstance, _hyperInstance->getCurrentPriority(), priority, timeout, name, args, imageData);
	connect(effect, &Effect::setInput, this, &EffectEngine::gotLedsHandler, Qt::QueuedConnection);
	connect(effect, &Effect::setInputImage, _hyperInstance, &HyperHdrInstance::setInputImage, Qt::QueuedConnection);
	connect(effect, &QThread::finished, this, &EffectEngine::effectFinished);
	connect(_hyperInstance, &HyperHdrInstance::finished, effect, &Effect::requestInterruption, Qt::DirectConnection);
	_activeEffects.push_back(effect);

	// start the effect
	Debug(_log, "Start the effect: name [%s], smoothCfg [%u]", QSTRING_CSTR(name), smoothCfg);
	_hyperInstance->registerInput(priority, hyperhdr::COMP_EFFECT, origin, name, smoothCfg);

	effect->start();

	return 0;
}

void EffectEngine::effectFinished()
{
	Effect* effect = qobject_cast<Effect*>(sender());
	if (!effect->isInterruptionRequested())
	{
		// effect stopped by itself. Clear the channel
		_hyperInstance->clear(effect->getPriority());
	}

	Info(_log, "Effect '%s' has finished.", QSTRING_CSTR(effect->getName()));
	for (auto effectIt = _activeEffects.begin(); effectIt != _activeEffects.end(); ++effectIt)
	{
		if (*effectIt == effect)
		{
			_activeEffects.erase(effectIt);
			break;
		}
	}

	// cleanup the effect
	effect->deleteLater();
}

void EffectEngine::visiblePriorityChanged(quint8 priority)
{
	for (Effect* effect : _activeEffects)
	{
		effect->visiblePriorityChanged(priority);
	}
}

std::list<EffectDefinition> EffectEngine::getEffects() const
{
	return _availableEffects;
}

std::list<ActiveEffectDefinition> EffectEngine::getActiveEffects() const
{
	std::list<ActiveEffectDefinition> availableActiveEffects;

	for (Effect* effect : _activeEffects)
	{
		ActiveEffectDefinition activeEffectDefinition;
		activeEffectDefinition.name = effect->getName();
		activeEffectDefinition.priority = effect->getPriority();
		activeEffectDefinition.timeout = effect->getTimeout();
		activeEffectDefinition.args = effect->getArgs();
		availableActiveEffects.push_back(activeEffectDefinition);
	}

	return availableActiveEffects;
}

void EffectEngine::gotLedsHandler(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms, bool clearEffect)
{
	int ledNum = _hyperInstance->getLedCount();
	if (ledNum == static_cast<int>(ledColors.size()))
		emit _hyperInstance->setInput(priority, ledColors, timeout_ms, false);
	else for (Effect* effect : _activeEffects)
	{
		effect->setLedCount(ledNum);
	}
}

void EffectEngine::cacheRunningEffects()
{
	_cachedActiveEffects.clear();

	for (Effect* effect : _activeEffects)
	{
		ActiveEffectDefinition activeEffectDefinition;
		activeEffectDefinition.name = effect->getName();
		activeEffectDefinition.priority = effect->getPriority();
		activeEffectDefinition.timeout = effect->getTimeout();
		activeEffectDefinition.args = effect->getArgs();
		_cachedActiveEffects.push_back(activeEffectDefinition);
		channelCleared(effect->getPriority());
	}
}

void EffectEngine::startCachedEffects()
{
	for (const auto& def : _cachedActiveEffects)
	{
		// the smooth cfg AND origin are ignored for this start!
		runEffect(def.name, def.args, def.priority, def.timeout);
	}
	_cachedActiveEffects.clear();
}

void EffectEngine::handleUpdatedEffectList()
{
	_availableEffects.clear();

	unsigned id = 2;
	unsigned dynamicId = 3;

	_hyperInstance->updateSmoothingConfig(id);

	for (auto def : _effectDBHandler->getEffects())
	{
		// add smoothing configs to HyperHdr
		if (def.args["smoothing-custom-settings"].toBool())
		{
			def.smoothCfg = _hyperInstance->updateSmoothingConfig(
				dynamicId++,
				def.args["smoothing-time_ms"].toInt(200),
				def.args["smoothing-updateFrequency"].toDouble(25),
				def.args["smoothing-direct-mode"].toBool(false));
		}
		else
		{
			def.smoothCfg = _hyperInstance->updateSmoothingConfig(id);
		}
		_availableEffects.push_back(def);
	}
}

int EffectEngine::runEffect(const QString& effectName, int priority, int timeout, const QString& origin)
{
	unsigned smoothCfg = 0;
	for (auto def : _availableEffects)
	{
		if (def.name == effectName)
		{
			smoothCfg = def.smoothCfg;
			break;
		}
	}
	return runEffect(effectName, QJsonObject(), priority, timeout, origin, smoothCfg);
}

int EffectEngine::runEffect(const QString& effectName, const QJsonObject& args, int priority, int timeout, const QString& origin, unsigned smoothCfg, const QString& imageData)
{
	Info(_log, "Run effect \"%s\" on channel %d", QSTRING_CSTR(effectName), priority);
	return runEffectScript(effectName, args, priority, timeout, origin, smoothCfg, imageData);
}

void EffectEngine::handleInitialEffect(HyperHdrInstance* hyperhdr, const QJsonObject& FGEffectConfig)
{
	const int FG_PRIORITY = 0;
	const int DURATION_INFINITY = 0;

	// initial foreground effect/color
	if (FGEffectConfig["enable"].toBool(true))
	{
		const QString fgTypeConfig = FGEffectConfig["type"].toString("effect");
		const QString fgEffectConfig = FGEffectConfig["effect"].toString("Rainbow swirl fast");
		const QJsonValue fgColorConfig = FGEffectConfig["color"];
		int default_fg_duration_ms = 3000;
		int fg_duration_ms = FGEffectConfig["duration_ms"].toInt(default_fg_duration_ms);
		if (fg_duration_ms == DURATION_INFINITY)
		{
			fg_duration_ms = default_fg_duration_ms;
			Warning(Logger::getInstance("HYPERHDR"), "foreground effect duration 'infinity' is forbidden, set to default value %d ms", default_fg_duration_ms);
		}
		if (fgTypeConfig.contains("color"))
		{
			auto FGCONFIG_ARRAY = fgColorConfig.toArray();
			std::vector<ColorRgb> fg_color = {
				ColorRgb {
					static_cast<uint8_t>(FGCONFIG_ARRAY.at(0).toInt(0)),
					static_cast<uint8_t>(FGCONFIG_ARRAY.at(1).toInt(0)),
					static_cast<uint8_t>(FGCONFIG_ARRAY.at(2).toInt(0))
				}
			};
			hyperhdr->setColor(FG_PRIORITY, fg_color, fg_duration_ms);
			Info(Logger::getInstance("HYPERHDR"), "Initial foreground color set (%d %d %d)", fg_color.at(0).red, fg_color.at(0).green, fg_color.at(0).blue);
		}
		else
		{
			int result = hyperhdr->setEffect(fgEffectConfig, FG_PRIORITY, fg_duration_ms);
			Info(Logger::getInstance("HYPERHDR"), "Initial foreground effect '%s' %s", QSTRING_CSTR(fgEffectConfig), ((result == 0) ? "started" : "failed"));
		}
	}
}

void EffectEngine::channelCleared(int priority)
{
	for (Effect* effect : _activeEffects)
	{
		if (effect->getPriority() == priority && !effect->isInterruptionRequested())
		{
			effect->requestInterruption();
		}
	}
}

void EffectEngine::allChannelsCleared()
{
	for (Effect* effect : _activeEffects)
	{
		if (effect->getPriority() != PriorityMuxer::LOWEST_EFFECT_PRIORITY && !effect->isInterruptionRequested())
		{
			effect->requestInterruption();
		}
	}
}
