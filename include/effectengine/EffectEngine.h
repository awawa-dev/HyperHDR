#pragma once

// Qt includes
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonArray>

#include <hyperhdrbase/HyperHdrInstance.h>

// Effect engine includes
#include <effectengine/EffectDefinition.h>
#include <effectengine/ActiveEffectDefinition.h>
#include <utils/Logger.h>

// pre-declaration
class Effect;
class EffectDBHandler;

class EffectEngine : public QObject
{
	Q_OBJECT

public:
	EffectEngine(HyperHdrInstance* hyperhdr);
	~EffectEngine() override;

	///
	/// @brief Get all init data of the running effects and stop them
	///
	void cacheRunningEffects();

	///
	/// @brief Start all cached effects, origin and smooth cfg is default
	///
	void startCachedEffects();

	static void handleInitialEffect(HyperHdrInstance* hyperhdr, const QJsonObject& FGEffectConfig);

signals:
	/// Emit when the effect list has been updated
	void effectListUpdated();

public slots:
	/// Run the specified effect on the given priority channel and optionally specify a timeout
	int runEffect(const QString& effectName, int priority, int timeout = -1, const QString& origin = "System");

	/// Run the specified effect on the given priority channel and optionally specify a timeout
	int runEffect(const QString& effectName
		, const QJsonObject& args
		, int priority
		, int timeout = -1

		, const QString& origin = "System"
		, unsigned smoothCfg = 0
		, const QString& imageData = ""
	);

	/// Clear any effect running on the provided channel
	void channelCleared(int priority);

	/// Clear all effects
	void allChannelsCleared();

	std::list<EffectDefinition> getEffects() const;

	std::list<ActiveEffectDefinition> getActiveEffects() const;

private slots:
	void effectFinished();

	///
	/// @brief is called whenever the EffectFileHandler emits updated effect list
	///
	void handleUpdatedEffectList();

private:
	/// Run the specified effect on the given priority channel and optionally specify a timeout
	int runEffectScript(
		const QString& name
		, const QJsonObject& args
		, int priority
		, int timeout = -1
		, const QString& origin = "System"
		, unsigned smoothCfg = 0
		, const QString& imageData = ""
	);

private:
	HyperHdrInstance* _hyperInstance;

	std::list<EffectDefinition> _availableEffects;

	std::list<Effect*> _activeEffects;

	std::list<ActiveEffectDefinition> _cachedActiveEffects;

	Logger* _log;

	// The global effect file handler
	EffectDBHandler* _effectDBHandler;
};
