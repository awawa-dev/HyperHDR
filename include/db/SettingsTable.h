#pragma once

#include <db/DBManager.h>

class SettingsTable : public DBManager
{

public:
	SettingsTable(quint8 instance);
	bool createSettingsRecord(const QString& type, const QString& config) const;
	bool recordExist(const QString& type) const;
	QJsonDocument getSettingsRecord(const QString& type) const;
	QString getSettingsRecordString(const QString& type) const;
	bool deleteSettingsRecordString(const QString& type) const;
	bool purge(const QString& type) const;
	void deleteInstance() const;
	bool isSettingGlobal(const QString& type) const;

private:
	const quint8 _instance;
};
