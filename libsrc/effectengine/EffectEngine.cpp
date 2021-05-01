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

EffectEngine::EffectEngine(HyperHdrInstance * hyperhdr)
	: _hyperInstance(hyperhdr)
	, _availableEffects()
	, _activeEffects()
	, _log(Logger::getInstance("EFFECTENGINE"))
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
	for (Effect * effect : _activeEffects)
	{		
		delete effect;
	}
}



std::list<ActiveEffectDefinition> EffectEngine::getActiveEffects() const
{
	std::list<ActiveEffectDefinition> availableActiveEffects;

	for (Effect * effect : _activeEffects)
	{
		ActiveEffectDefinition activeEffectDefinition;
		activeEffectDefinition.name     = effect->getName();
		activeEffectDefinition.priority = effect->getPriority();
		activeEffectDefinition.timeout  = effect->getTimeout();
		activeEffectDefinition.args     = effect->getArgs();
		availableActiveEffects.push_back(activeEffectDefinition);
	}

	return availableActiveEffects;
}



void EffectEngine::cacheRunningEffects()
{
	_cachedActiveEffects.clear();

	for (Effect * effect : _activeEffects)
	{
		ActiveEffectDefinition activeEffectDefinition;
		activeEffectDefinition.name      = effect->getName();
		activeEffectDefinition.priority  = effect->getPriority();
		activeEffectDefinition.timeout   = effect->getTimeout();
		activeEffectDefinition.args      = effect->getArgs();
		_cachedActiveEffects.push_back(activeEffectDefinition);
		channelCleared(effect->getPriority());
	}
}

void EffectEngine::startCachedEffects()
{
	for (const auto & def : _cachedActiveEffects)
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
	emit effectListUpdated();
}

int EffectEngine::runEffect(const QString &effectName, int priority, int timeout, const QString &origin)
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

int EffectEngine::runEffect(const QString &effectName, const QJsonObject &args, int priority, int timeout, const QString &origin, unsigned smoothCfg, const QString &imageData)
{	
	Info( _log, "Run effect \"%s\" on channel %d", QSTRING_CSTR(effectName), priority);
	return runEffectScript(effectName, args, priority, timeout, origin, smoothCfg, imageData);
}

int EffectEngine::runEffectScript(const QString &name, const QJsonObject &args, int priority, int timeout, const QString &origin, unsigned smoothCfg, const QString &imageData)
{
	// clear current effect on the channel
	channelCleared(priority);

	// create the effect
	Effect *effect = new Effect(_hyperInstance, priority, timeout, name, args, imageData);
	connect(effect, &Effect::setInput, _hyperInstance, &HyperHdrInstance::setInput, Qt::QueuedConnection);
	connect(effect, &Effect::setInputImage, _hyperInstance, &HyperHdrInstance::setInputImage, Qt::QueuedConnection);
	connect(effect, &QThread::finished, this, &EffectEngine::effectFinished);
	connect(_hyperInstance, &HyperHdrInstance::finished, effect, &Effect::requestInterruption, Qt::DirectConnection);
	_activeEffects.push_back(effect);

	// start the effect
	Debug(_log, "Start the effect: name [%s], smoothCfg [%u]", QSTRING_CSTR(name), smoothCfg);
	_hyperInstance->registerInput(priority, hyperhdr::COMP_EFFECT, origin, name ,smoothCfg);

	effect->start();

	return 0;
}

void EffectEngine::channelCleared(int priority)
{
	for (Effect * effect : _activeEffects)
	{
		if (effect->getPriority() == priority && !effect->isInterruptionRequested())
		{
			effect->requestInterruption();
		}
	}
}

void EffectEngine::allChannelsCleared()
{
	for (Effect * effect : _activeEffects)
	{
		if (effect->getPriority() != 254 && !effect->isInterruptionRequested())
		{
			effect->requestInterruption();
		}
	}
}

void EffectEngine::effectFinished()
{
	Effect* effect = qobject_cast<Effect*>(sender());
	if (!effect->isInterruptionRequested())
	{
		// effect stopped by itself. Clear the channel
		_hyperInstance->clear(effect->getPriority());
	}
	
	Info( _log, "Effect '%s' has finished.",QSTRING_CSTR(effect->getName()));
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
