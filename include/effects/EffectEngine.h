#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <QJsonObject>
	#include <QJsonValue>
	#include <QJsonDocument>
	#include <QJsonArray>
#endif

#include <base/HyperHdrInstance.h>
#include <effects/EffectDefinition.h>
#include <effects/ActiveEffectDefinition.h>
#include <utils/Logger.h>

class Effect;
class EffectDBHandler;

class EffectEngine : public QObject
{
	Q_OBJECT

public:
	EffectEngine(HyperHdrInstance* hyperhdr);
	~EffectEngine() override;

public slots:
	int runEffect(const QString& effectName, int priority, int timeout = -1, const QString& origin = "System");
	void channelCleared(int priority);
	void allChannelsCleared();
	std::list<EffectDefinition> getEffects() const;
	std::list<ActiveEffectDefinition> getActiveEffects() const;
	void visiblePriorityChanged(quint8 priority);
	void handlerSetLeds(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms = -1, bool clearEffect = true);

private slots:
	void handlerEffectFinished(int priority, QString name, bool forced);


private:
	int runEffectScript(const QString& name, int priority, int timeout, const QString& origin);
	void createSmoothingConfigs();

	HyperHdrInstance* _hyperInstance;
	std::list<EffectDefinition> _availableEffects;
	std::list<std::unique_ptr<Effect, void(*)(Effect*)>> _activeEffects;

	Logger* _log;
	EffectDBHandler* _effectDBHandler;
};
