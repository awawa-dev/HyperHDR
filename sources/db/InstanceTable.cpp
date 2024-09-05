#include <db/InstanceTable.h>

InstanceTable::InstanceTable()
	: DBManager()
{
	// Init instance table
	setTable("instances");
	createTable(QStringList() << "instance INTEGER" << "friendly_name TEXT" << "enabled INTEGER DEFAULT 0" << "last_use TEXT");

	createInstance();
};


bool InstanceTable::createInstance(const QString& name, quint8& inst)
{
	VectorPair fcond;
	fcond.append(CPair("friendly_name", name));

	// check duplicate
	if (!recordExists(fcond))
	{
		inst = 0;
		VectorPair cond;
		cond.append(CPair("instance", inst));

		// increment to next avail index
		while (recordExists(cond))
		{
			inst++;
			cond.removeFirst();
			cond.append(CPair("instance", inst));
		}
		// create
		QVariantMap data;
		data["friendly_name"] = name;
		data["instance"] = inst;
		VectorPair lcond;
		return createRecord(lcond, data);
	}
	return false;
}


bool InstanceTable::deleteInstance(quint8 inst)
{
	VectorPair cond;
	cond.append(CPair("instance", inst));
	if (deleteRecord(cond))
	{
		// delete settings entries
		SettingsTable settingsTable(inst);
		settingsTable.deleteInstance();
		return true;
	}
	return false;
}

bool InstanceTable::saveName(quint8 inst, const QString& name)
{
	VectorPair fcond;
	fcond.append(CPair("friendly_name", name));

	// check duplicate
	if (!recordExists(fcond))
	{
		if (instanceExist(inst))
		{
			VectorPair cond;
			cond.append(CPair("instance", inst));
			QVariantMap data;
			data["friendly_name"] = name;

			return updateRecord(cond, data);
		}
	}
	return false;
}


QVector<QVariantMap> InstanceTable::getAllInstances(bool justEnabled)
{
	QVector<QVariantMap> results;
	getRecords(results, QStringList(), QStringList() << "instance ASC");
	if (justEnabled)
	{
		for (auto it = results.begin(); it != results.end();)
		{
			if (!(*it)["enabled"].toBool())
			{
				it = results.erase(it);
				continue;
			}
			++it;
		}
	}
	return results;
}

bool InstanceTable::instanceExist(quint8 inst)
{
	VectorPair cond;
	cond.append(CPair("instance", inst));
	return recordExists(cond);
}

const QString InstanceTable::getNamebyIndex(quint8 index)
{
	QVariantMap results;
	VectorPair cond;
	cond.append(CPair("instance", index));
	getRecord(cond, results, QStringList("friendly_name"));

	QString name = results["friendly_name"].toString();
	return name.isEmpty() ? "NOT FOUND" : name;
}

void InstanceTable::setLastUse(quint8 inst)
{
	VectorPair cond;
	cond.append(CPair("instance", inst));
	QVariantMap map;
	map["last_use"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	updateRecord(cond, map);
}

void InstanceTable::setEnable(quint8 inst, bool newState)
{
	VectorPair cond;
	cond.append(CPair("instance", inst));
	QVariantMap map;
	map["enabled"] = newState;
	updateRecord(cond, map);
}

bool InstanceTable::isEnabled(quint8 inst)
{
	VectorPair cond;
	cond.append(CPair("instance", inst));
	QVariantMap results;
	getRecord(cond, results);

	return results["enabled"].toBool();
}

void InstanceTable::createInstance()
{
	if (instanceExist(0))
		setEnable(0, true);
	else
	{
		QVariantMap data;
		data["friendly_name"] = "First LED instance";
		VectorPair cond;
		cond.append(CPair("instance", 0));
		if (createRecord(cond, data))
			setEnable(0, true);
		else
			throw std::runtime_error("Failed to create HyperHDR root instance in db! This should never be the case...");
	}
}

