/* DBManager.cpp
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

#include <db/DBManager.h>
#include <utils/settings.h>

#include <QSqlQuery>
#include <QDir>
#include <QSqlError>
#include <QJsonArray>
#include <QSqlRecord>
#include <QDateTime>
#include <QSqlDatabase>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QIODevice>
#include <iostream>
#include <sqlite3.h>

#include <HyperhdrConfig.h> // Required to determine the cmake options

QFileInfo DBManager::_databaseName;
QThreadStorage<QSqlDatabase> DBManager::_databasePool;

DBManager::DBManager()
	: _log(Logger::getInstance("DB"))
	, _readonlyMode(false)
{
	std::cout << "SQLite version: " << sqlite3_libversion() << std::endl;
}

DBManager::~DBManager()
{
}

void DBManager::setReadonlyMode(bool readOnly)
{
	_readonlyMode = readOnly;
};

void DBManager::initializeDatabaseFilename(QFileInfo databaseName)
{
	_databaseName = databaseName;
}

void DBManager::setTable(const QString& table)
{
	_table = table;
}

QSqlDatabase DBManager::getDB() const
{
	if (_databasePool.hasLocalData())
		return _databasePool.localData();
	else
	{
		auto db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
		_databasePool.setLocalData(db);
		
		db.setDatabaseName(_databaseName.absoluteFilePath());
		if (!db.open())
		{
			Error(_log, QSTRING_CSTR(db.lastError().text()));
			throw std::runtime_error("Failed to open database connection!");
		}
		else
			Info(_log, "Database opened: %s", QSTRING_CSTR(_databaseName.absoluteFilePath()));

		return db;
	}
}

bool DBManager::createRecord(const VectorPair& conditions, const QVariantMap& columns) const
{
	if (_readonlyMode)
	{
		return false;
	}

	if (recordExists(conditions))
	{
		// if there is no column data, return
		if (columns.isEmpty())
			return true;

		if (!updateRecord(conditions, columns))
			return false;

		return true;
	}

	QSqlDatabase idb = getDB();
	QSqlQuery query(idb);
	query.setForwardOnly(true);

	QVariantList cValues;
	QStringList prep;
	QStringList placeh;
	// prep merge columns & condition
	QVariantMap::const_iterator i = columns.constBegin();
	while (i != columns.constEnd()) {
		prep.append(i.key());
		cValues += i.value();
		placeh.append("?");

		++i;
	}
	for (const auto& pair : conditions)
	{
		// remove the condition statements
		QString tmp = pair.first;
		prep << tmp.remove("AND");
		cValues << pair.second;
		placeh.append("?");
	}
	query.prepare(QString("INSERT INTO %1 ( %2 ) VALUES ( %3 )").arg(_table, prep.join(", ")).arg(placeh.join(", ")));
	// add column & condition values
	doAddBindValue(query, cValues);
	if (!query.exec())
	{
		Error(_log, "Failed to create record: '%s' in table: '%s' Error: %s", QSTRING_CSTR(prep.join(", ")), QSTRING_CSTR(_table), QSTRING_CSTR(idb.lastError().text()));
		return false;
	}
	return true;
}

bool DBManager::recordExists(const VectorPair& conditions) const
{
	if (conditions.isEmpty())
		return false;

	QSqlDatabase idb = getDB();
	QSqlQuery query(idb);
	query.setForwardOnly(true);

	QStringList prepCond;
	QVariantList bindVal;
	prepCond << "WHERE";

	for (const auto& pair : conditions)
	{
		prepCond << pair.first + "=?";
		bindVal << pair.second;
	}
	query.prepare(QString("SELECT * FROM %1 %2").arg(_table, prepCond.join(" ")));
	doAddBindValue(query, bindVal);
	if (!query.exec())
	{
		Error(_log, "Failed recordExists(): '%s' in table: '%s' Error: %s", QSTRING_CSTR(prepCond.join(" ")), QSTRING_CSTR(_table), QSTRING_CSTR(idb.lastError().text()));
		return false;
	}

	int entry = 0;
	while (query.next()) {
		entry++;
	}

	if (entry)
		return true;

	return false;
}

bool DBManager::updateRecord(const VectorPair& conditions, const QVariantMap& columns) const
{
	if (_readonlyMode)
	{
		return false;
	}

	QSqlDatabase idb = getDB();
	if (idb.transaction())
	{

		QSqlQuery query(idb);
		query.setForwardOnly(true);

		QVariantList values;
		QStringList prep;

		// prepare columns valus
		QVariantMap::const_iterator i = columns.constBegin();
		while (i != columns.constEnd()) {
			prep += i.key() + "=?";
			values += i.value();

			++i;
		}

		// prepare condition values
		QStringList prepCond;
		QVariantList prepBindVal;
		if (!conditions.isEmpty())
			prepCond << "WHERE";

		for (const auto& pair : conditions)
		{
			prepCond << pair.first + "=?";
			prepBindVal << pair.second;
		}

		query.prepare(QString("UPDATE %1 SET %2 %3").arg(_table, prep.join(", ")).arg(prepCond.join(" ")));
		// add column values
		doAddBindValue(query, values);
		// add condition values
		doAddBindValue(query, prepBindVal);
		if (!query.exec())
		{
			Error(_log, "Failed to update record: '%s' in table: '%s' Error: %s", QSTRING_CSTR(prepCond.join(" ")), QSTRING_CSTR(_table), QSTRING_CSTR(idb.lastError().text()));
			idb.rollback();
			return false;
		}
	}
	else
	{
		Error(_log, "Could not create a DB transaction. Error: %s", QSTRING_CSTR(idb.lastError().text()));
		return false;
	}

	if (!idb.commit())
	{
		Error(_log, "Could not commit the DB transaction. Error: %s", QSTRING_CSTR(idb.lastError().text()));
	}

	return true;
}

bool DBManager::getRecord(const VectorPair& conditions, QVariantMap& results, const QStringList& tColumns, const QStringList& tOrder) const
{
	QSqlDatabase idb = getDB();
	QSqlQuery query(idb);
	query.setForwardOnly(true);

	QString sColumns("*");
	if (!tColumns.isEmpty())
		sColumns = tColumns.join(", ");

	QString sOrder("");
	if (!tOrder.isEmpty())
	{
		sOrder = " ORDER BY ";
		sOrder.append(tOrder.join(", "));
	}
	// prep conditions
	QStringList prepCond;
	QVariantList bindVal;
	if (!conditions.isEmpty())
		prepCond << " WHERE";

	for (const auto& pair : conditions)
	{
		prepCond << pair.first + "=?";
		bindVal << pair.second;
	}
	query.prepare(QString("SELECT %1 FROM %2%3%4").arg(sColumns, _table).arg(prepCond.join(" ")).arg(sOrder));
	doAddBindValue(query, bindVal);

	if (!query.exec())
	{
		Error(_log, "Failed to get record: '%s' in table: '%s' Error: %s", QSTRING_CSTR(prepCond.join(" ")), QSTRING_CSTR(_table), QSTRING_CSTR(idb.lastError().text()));
		return false;
	}

	// go to first row
	query.next();

	QSqlRecord rec = query.record();
	for (int i = 0; i < rec.count(); i++)
	{
		results[rec.fieldName(i)] = rec.value(i);
	}

	return true;
}

bool DBManager::getRecords(QVector<QVariantMap>& results, const QStringList& tColumns, const QStringList& tOrder) const
{
	QSqlDatabase idb = getDB();
	QSqlQuery query(idb);
	query.setForwardOnly(true);

	QString sColumns("*");
	if (!tColumns.isEmpty())
		sColumns = tColumns.join(", ");

	QString sOrder("");
	if (!tOrder.isEmpty())
	{
		sOrder = " ORDER BY ";
		sOrder.append(tOrder.join(", "));
	}

	query.prepare(QString("SELECT %1 FROM %2%3").arg(sColumns, _table, sOrder));

	if (!query.exec())
	{
		Error(_log, "Failed to get records: '%s' in table: '%s' Error: %s", QSTRING_CSTR(sColumns), QSTRING_CSTR(_table), QSTRING_CSTR(idb.lastError().text()));
		return false;
	}

	// iterate through all found records
	while (query.next())
	{
		QVariantMap entry;
		QSqlRecord rec = query.record();
		for (int i = 0; i < rec.count(); i++)
		{
			entry[rec.fieldName(i)] = rec.value(i);
		}
		results.append(entry);
	}

	return true;
}


bool DBManager::deleteRecord(const VectorPair& conditions) const
{
	if (_readonlyMode)
	{
		return false;
	}

	if (conditions.isEmpty())
	{
		Error(_log, "Oops, a deleteRecord() call wants to delete the entire table (%s)! Denied it", QSTRING_CSTR(_table));
		return false;
	}

	if (recordExists(conditions))
	{
		QSqlDatabase idb = getDB();
		QSqlQuery query(idb);

		// prep conditions
		QStringList prepCond("WHERE");
		QVariantList bindValues;

		for (const auto& pair : conditions)
		{
			prepCond << pair.first + "=?";
			bindValues << pair.second;
		}

		query.prepare(QString("DELETE FROM %1 %2").arg(_table, prepCond.join(" ")));
		doAddBindValue(query, bindValues);
		if (!query.exec())
		{
			Error(_log, "Failed to delete record: '%s' in table: '%s' Error: %s", QSTRING_CSTR(prepCond.join(" ")), QSTRING_CSTR(_table), QSTRING_CSTR(idb.lastError().text()));
			return false;
		}
		return true;
	}
	return false;
}

bool DBManager::createTable(QStringList& columns) const
{
	if (_readonlyMode)
	{
		return false;
	}

	if (columns.isEmpty())
	{
		Error(_log, "Empty tables aren't supported!");
		return false;
	}

	QSqlDatabase idb = getDB();
	// create table if required
	QSqlQuery query(idb);
	if (!tableExists(_table))
	{
		// empty tables aren't supported by sqlite, add one column
		QString tcolumn = columns.takeFirst();
		// default CURRENT_TIMESTAMP is not supported by ALTER TABLE
		if (!query.exec(QString("CREATE TABLE %1 ( %2 )").arg(_table, tcolumn)))
		{
			Error(_log, "Failed to create table: '%s' Error: %s", QSTRING_CSTR(_table), QSTRING_CSTR(idb.lastError().text()));
			return false;
		}
	}
	// create columns if required
	QSqlRecord rec = idb.record(_table);
	int err = 0;
	for (const auto& column : columns)
	{
		QStringList id = column.split(' ');
		if (rec.indexOf(id.at(0)) == -1)
		{
			if (!createColumn(column))
			{
				err++;
			}
		}
	}
	if (err)
		return false;

	return true;
}

bool DBManager::createColumn(const QString& column) const
{
	if (_readonlyMode)
	{
		return false;
	}

	QSqlDatabase idb = getDB();
	QSqlQuery query(idb);
	if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2").arg(_table, column)))
	{
		Error(_log, "Failed to create column: '%s' in table: '%s' Error: %s", QSTRING_CSTR(column), QSTRING_CSTR(_table), QSTRING_CSTR(idb.lastError().text()));
		return false;
	}
	return true;
}

bool DBManager::tableExists(const QString& table) const
{
	QSqlDatabase idb = getDB();
	QStringList tables = idb.tables();
	if (tables.contains(table))
		return true;
	return false;
}

bool DBManager::deleteTable(const QString& table) const
{
	if (_readonlyMode)
	{
		return false;
	}

	if (tableExists(table))
	{
		QSqlDatabase idb = getDB();
		QSqlQuery query(idb);
		if (!query.exec(QString("DROP TABLE %1").arg(table)))
		{
			Error(_log, "Failed to delete table: '%s' Error: %s", QSTRING_CSTR(table), QSTRING_CSTR(idb.lastError().text()));
			return false;
		}
	}
	return true;
}

void DBManager::doAddBindValue(QSqlQuery& query, const QVariantList& variants) const
{
	for (const auto& variant : variants)
	{
		auto t = variant.userType();
		switch (t)
		{
			case QMetaType::UInt:
			case QMetaType::Int:
			case QMetaType::Bool:
				query.addBindValue(variant.toInt());
				break;
			case QMetaType::Double:
				query.addBindValue(variant.toFloat());
				break;
			case QMetaType::QByteArray:
				query.addBindValue(variant.toByteArray());
				break;
			default:
				query.addBindValue(variant.toString());
				break;
		}
	}
}

const QJsonObject DBManager::getBackup()
{
	QJsonObject backup;
	QSqlDatabase idb = getDB();
	QStringList instanceKeys({ "enabled", "friendly_name", "instance" });
	QStringList settingsKeys({ "config", "hyperhdr_instance", "type" });

	bool rm = _readonlyMode;

	_readonlyMode = true;

	QSqlQuery queryInst(idb);
	queryInst.setForwardOnly(true);

	queryInst.prepare(QString("SELECT * FROM instances"));
	if (!queryInst.exec())
	{
		Error(_log, "Failed to get records from instances table");
		_readonlyMode = rm;
		return backup;
	}

	QSqlQuery querySet(idb);
	querySet.setForwardOnly(true);

	querySet.prepare(QString("SELECT * FROM settings"));
	if (!querySet.exec())
	{
		Error(_log, "Failed to get records from settings table");
		_readonlyMode = rm;
		return backup;
	}

	// iterate through all found records
	QJsonArray allInstances;
	while (queryInst.next())
	{
		QJsonObject entry;
		QSqlRecord rec = queryInst.record();
		for (int i = 0; i < rec.count(); i++)
			if (instanceKeys.contains(rec.fieldName(i), Qt::CaseInsensitive) && !rec.value(i).isNull())
			{
				entry[rec.fieldName(i)] = QJsonValue::fromVariant(rec.value(i));
			}
		allInstances.append(entry);
	}


	QJsonArray allSettings;
	while (querySet.next())
	{
		QJsonObject entry;
		QSqlRecord rec = querySet.record();
		bool valid = false;

		for (int i = 0; i < rec.count(); i++)
			if (settingsKeys.contains(rec.fieldName(i), Qt::CaseInsensitive) && !rec.value(i).isNull())
			{
				QVariant column = rec.value(i);

				if (rec.fieldName(i) == "type")
				{
					for (settings::type selector = settings::type::SNDEFFECT; !valid && selector != settings::type::INVALID; selector = settings::type(((int)selector) + 1))
					{
						if (QString::compare(typeToString(selector), column.toString(), Qt::CaseInsensitive) == 0)
							valid = true;
					}
				}

				entry[rec.fieldName(i)] = QJsonValue::fromVariant(column);
			}

		if (valid)
			allSettings.append(entry);
	}

	backup["version"] = CURRENT_HYPERHDR_DB_EXPORT_VERSION;
	backup["instances"] = allInstances;
	backup["settings"] = allSettings;

	_readonlyMode = rm;
	return backup;
}

QString DBManager::restoreBackup(const QJsonObject& backupData)
{
	QSqlDatabase idb = getDB();
	const QJsonObject& message = backupData.value("config").toObject();
	bool rm = _readonlyMode;	

	Info(_log, "Creating DB backup first.");
	QString resultFile = createLocalBackup();
	if (!resultFile.isEmpty())
		Info(_log, "The backup is saved as: %s", QSTRING_CSTR(resultFile));
	else
		Warning(_log, "Could not create a backup");

	_readonlyMode = true;

	if (idb.transaction())
	{
		QSqlQuery queryDelInstances(idb);
		queryDelInstances.prepare(QString("DELETE FROM instances"));

		QSqlQuery queryDelSettings(idb);
		queryDelSettings.prepare(QString("DELETE FROM settings"));

		if (!queryDelSettings.exec() || !queryDelInstances.exec())
		{
			Error(_log, "Failed to clear tables before import");
			idb.rollback();
			_readonlyMode = rm;
			return "Failed to clear tables before import";
		}

		const QJsonArray instances = message.value("instances").toArray();
		if (instances.count() > 0)
		{
			for (auto value : instances)
			{
				QStringList headers, placeholder;
				QVariantList values;
				QJsonObject obj = value.toObject();

				foreach(const QString & key, obj.keys())
				{
					headers.append(key);
					placeholder.append("?");
					if (obj.value(key).isString())
						values.append(obj.value(key).toString());
					else
						values.append(obj.value(key).toInt());
				}

				QSqlQuery queryInstInsert(idb);
				queryInstInsert.prepare(QString("INSERT INTO %1 ( %2 ) VALUES ( %3 )").arg("instances").arg(headers.join(", ")).arg(placeholder.join(", ")));
				doAddBindValue(queryInstInsert, values);

				if (!queryInstInsert.exec())
				{
					Error(_log, "Failed to create record in table 'instances'. Error: %s", QSTRING_CSTR(idb.lastError().text()));
					idb.rollback();
					_readonlyMode = rm;
					return "Failed to create record in table 'instances': " + idb.lastError().text();
				}
			}
		}

		const QJsonArray settings = message.value("settings").toArray();
		if (settings.count() > 0)
		{
			for (auto value : settings)
			{
				QStringList headers, placeholder;
				QVariantList values;
				QJsonObject obj = value.toObject();

				foreach(const QString & key, obj.keys())
				{
					headers.append(key);
					placeholder.append("?");
					if (obj.value(key).isString())
						values.append(obj.value(key).toString());
					else
						values.append(obj.value(key).toInt());
				}

				QSqlQuery querySetInsert(idb);
				querySetInsert.prepare(QString("INSERT INTO %1 ( %2 ) VALUES ( %3 )").arg("settings").arg(headers.join(", ")).arg(placeholder.join(", ")));
				doAddBindValue(querySetInsert, values);

				if (!querySetInsert.exec())
				{
					Error(_log, "Failed to create record in table 'settings'. Error: %s", QSTRING_CSTR(idb.lastError().text()));
					idb.rollback();
					_readonlyMode = rm;
					return "Failed to create record in table 'settings': " + idb.lastError().text();
				}
			}
		}
	}
	else
	{
		Error(_log, "Could not create a DB transaction. Error: %s", QSTRING_CSTR(idb.lastError().text()));
		_readonlyMode = rm;
		return  "Could not create a DB transaction. Error: " + idb.lastError().text();
	}

	if (!idb.commit())
	{
		Error(_log, "Could not commit the DB transaction. Error: %s", QSTRING_CSTR(idb.lastError().text()));
		_readonlyMode = rm;
		return  "Could not commit the DB transaction. Error: " + idb.lastError().text();
	}

	return "";
}


QString DBManager::createLocalBackup()
{
	QJsonObject backupFirst = getBackup();
	QString backupName = getDB().databaseName();
	if (!backupName.isEmpty() && QFile::exists(backupName))
	{
		backupName = QDir(QFileInfo(backupName).absoluteDir()).filePath(QString("backup_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmsszzz")));
		QFile backFile(backupName);
		if (backFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		{
			QTextStream out(&backFile);
			out.setGenerateByteOrderMark(true);
			out << QJsonDocument(backupFirst).toJson(QJsonDocument::Compact);
			out.flush();
			backFile.close();
			return backupName;
		}
	}
	return QString();
}
