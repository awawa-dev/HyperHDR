/* EffectEngine.cpp
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
	#include <QResource>
#endif

#include <HyperhdrConfig.h>

#include <base/Muxer.h>
#include <utils/Components.h>
#include <base/Smoothing.h>

#include <effects/EffectEngine.h>
#include <effects/Effect.h>

EffectEngine::EffectEngine(HyperHdrInstance* hyperhdr)
	: _hyperInstance(hyperhdr)
	, _availableEffects(Effect::getAvailableEffects())
	, _log(Logger::getInstance(QString("EFFECTENGINE%1").arg(hyperhdr->getInstanceIndex())))
{
	qRegisterMetaType<hyperhdr::Components>("hyperhdr::Components");

	// register smooth cfgs and fill available effects
	createSmoothingConfigs();
}

EffectEngine::~EffectEngine()
{
	_activeEffects.clear();	

	Debug(_log, "EffectEngine is released");
}

int EffectEngine::runEffectScript(const QString& name, int priority, int timeout, const QString& origin)
{
	// find the effect's definition
	std::string findName = name.toStdString();
	std::list<EffectDefinition>::iterator it = std::find_if(
		_availableEffects.begin(),
		_availableEffects.end(),
		[&findName](const EffectDefinition& s) {
			return s.name == findName;
		});

	if (it == _availableEffects.end())
	{
		Debug(_log, "Could not find the effect named: %s", QSTRING_CSTR(name));
		return 0;
	}

	// clear current effect on the channel
	channelCleared(priority);

	// create the effect
	auto effect = std::unique_ptr<Effect, void(*)(Effect*)>(
		new Effect(_hyperInstance, _hyperInstance->getCurrentPriority(), priority, timeout, *it),
		[](Effect* oldEffect) {
			oldEffect->requestInterruption();
			hyperhdr::THREAD_REMOVER(oldEffect->getDescription(), oldEffect->thread(), oldEffect);
		}
	);
	connect(effect.get(), &Effect::SignalSetLeds, this, &EffectEngine::handlerSetLeds);
	connect(effect.get(), &Effect::SignalSetImage, _hyperInstance, &HyperHdrInstance::setInputImage);
	connect(effect.get(), &Effect::SignalEffectFinished, this, &EffectEngine::handlerEffectFinished);

	// start the effect
	Debug(_log, "Start the effect: name [%s]", QSTRING_CSTR(name));
	_hyperInstance->registerInput(priority, hyperhdr::COMP_EFFECT, origin, name, (*it).smoothingConfig);

	// start the effect
	QThread* newThread = new QThread();
	effect->moveToThread(newThread);
	newThread->start();
	QUEUE_CALL_0(effect.get(), start);

	// enlist & move control
	_activeEffects.push_back(std::move(effect));

	return 0;
}

void EffectEngine::handlerEffectFinished(int priority, QString name, bool forced)
{
	if (!forced)
	{
		_hyperInstance->clear(priority);
	}

	Info(_log, "Effect '%s' has finished.", QSTRING_CSTR(name));
	for (auto effectIt = _activeEffects.begin(); effectIt != _activeEffects.end(); ++effectIt)
	{
		if ((*effectIt).get() == sender())
		{
			_activeEffects.erase(effectIt);
			break;
		}
	}
}

void EffectEngine::visiblePriorityChanged(quint8 priority)
{
	for (auto&& effect : _activeEffects)
	{
		QUEUE_CALL_1(effect.get(), visiblePriorityChanged, quint8, priority);
	}
}

std::list<EffectDefinition> EffectEngine::getEffects() const
{
	return _availableEffects;
}

std::list<ActiveEffectDefinition> EffectEngine::getActiveEffects() const
{
	std::list<ActiveEffectDefinition> availableActiveEffects;

	for (const auto& effect : _activeEffects)
	{
		ActiveEffectDefinition activeEffectDefinition;
		activeEffectDefinition.name = effect->getName();
		activeEffectDefinition.priority = effect->getPriority();
		activeEffectDefinition.timeout = effect->getTimeout();
		availableActiveEffects.push_back(activeEffectDefinition);
	}

	return availableActiveEffects;
}

void EffectEngine::handlerSetLeds(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms, bool clearEffect)
{
	int ledNum = _hyperInstance->getLedCount();
	if (ledNum == static_cast<int>(ledColors.size()))
		_hyperInstance->setInputLeds(priority, ledColors, timeout_ms, false);
	else for (auto&& effect : _activeEffects)
	{
		effect->setLedCount(ledNum);
	}
}

void EffectEngine::createSmoothingConfigs()
{
	unsigned defaultEffectConfig = SMOOTHING_EFFECT_CONFIGS_START;
	unsigned dynamicId = defaultEffectConfig + 1;

	_hyperInstance->addEffectConfig(defaultEffectConfig);

	for (EffectDefinition& def : _availableEffects)
	{
		// add smoothing configs to HyperHdr
		if (def.smoothingCustomSettings)
		{
			def.smoothingConfig = _hyperInstance->addEffectConfig(
				dynamicId++,
				def.smoothingTime,
				def.smoothingFrequency,
				def.smoothingDirectMode);
		}
		else
		{
			def.smoothingConfig = _hyperInstance->addEffectConfig(defaultEffectConfig);
		}
	}
}

int EffectEngine::runEffect(const QString& effectName, int priority, int timeout, const QString& origin)
{
	Info(_log, "Run effect \"%s\" on channel %d", QSTRING_CSTR(effectName), priority);
	return runEffectScript(effectName, priority, timeout, origin);
}

void EffectEngine::channelCleared(int priority)
{
	for (auto&& effect : _activeEffects)
	{
		if (effect->getPriority() == priority)
		{
			effect->requestInterruption();
		}
	}
}

void EffectEngine::allChannelsCleared()
{
	for (auto&& effect : _activeEffects)
	{
		if (effect->getPriority() != Muxer::LOWEST_EFFECT_PRIORITY)
		{
			effect->requestInterruption();
		}
	}
}
