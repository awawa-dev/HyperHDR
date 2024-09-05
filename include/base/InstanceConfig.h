#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <QMutexLocker>

	#include <atomic>
#endif

#include <utils/Logger.h>
#include <utils/settings.h>

class SettingsTable;

class InstanceConfig : public QObject
{
	Q_OBJECT

public:
	InstanceConfig(bool master, quint8 instanceIndex, QObject* parent);
	~InstanceConfig();

	bool upgradeDB(QJsonObject& dbConfig);
	bool saveSettings(QJsonObject config, bool correct = false);
	QJsonDocument getSetting(settings::type type) const;
	void saveSetting(settings::type key, QString saveData);
	QJsonObject getSettings() const;
	bool isReadOnlyMode();
signals:
	void SignalInstanceSettingsChanged(settings::type type, const QJsonDocument& data);

private:
	Logger*			_log;
	std::unique_ptr<SettingsTable> _sTable;
	QJsonObject		_qconfig;

	static QJsonObject			_schemaJson;
	static QMutex				_lockerSettingsManager;
	static std::atomic<bool>	_backupMade;	
};
