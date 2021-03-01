#pragma once

// util
#include <utils/Logger.h>
#include <effectengine/EffectDefinition.h>
#include <utils/settings.h>

class EffectDBHandler : public QObject
{
	Q_OBJECT
private:
	friend class HyperHdrDaemon;
	EffectDBHandler(const QString& rootPath, const QJsonDocument& effectConfig, QObject* parent = nullptr);
	static EffectDBHandler*	efhInstance;

public:	
	static EffectDBHandler*	getInstance();
	std::list<EffectDefinition> getEffects() const;
	
public slots:
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

signals:
	void effectListChanged();

private:
	void updateEffects();	

private:
	QJsonObject		_effectConfig;
	Logger*			_log;
	const QString	_rootPath;

	// available effects
	std::list<EffectDefinition> _availableEffects;	
};
