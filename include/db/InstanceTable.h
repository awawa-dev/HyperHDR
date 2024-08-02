#pragma once

#include <db/DBManager.h>
#include <db/SettingsTable.h>

class InstanceTable : public DBManager
{

public:
	InstanceTable();

	bool createInstance(const QString& name, quint8& inst);
	bool deleteInstance(quint8 inst);
	bool saveName(quint8 inst, const QString& name);
	QVector<QVariantMap> getAllInstances(bool justEnabled = false);
	bool instanceExist(quint8 inst);
	const QString getNamebyIndex(quint8 index);
	void setLastUse(quint8 inst);
	void setEnable(quint8 inst, bool newState);
	bool isEnabled(quint8 inst);

private:
	void createInstance();
};
