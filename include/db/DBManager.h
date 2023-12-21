#pragma once

#ifndef PCH_ENABLED
	#include <QFileInfo>
	#include <QMap>
	#include <QVariant>
	#include <QPair>
	#include <QVector>
	#include <QJsonObject>
	#include <QJsonDocument>
#endif

#include <QThreadStorage>

#include <utils/Logger.h>

class QSqlDatabase;
class QSqlQuery;

typedef QPair<QString, QVariant> CPair;
typedef QVector<CPair> VectorPair;

#define CURRENT_HYPERHDR_DB_EXPORT_VERSION "HyperHDR_export_format_v20"

class DBManager
{

public:	
	virtual ~DBManager();

	static void initializeDatabaseFilename(QFileInfo databaseName);
	void setTable(const QString& table);

	QSqlDatabase getDB() const;

	bool createTable(QStringList& columns) const;
	bool createColumn(const QString& column) const;
	bool recordExists(const VectorPair& conditions) const;
	bool createRecord(const VectorPair& conditions, const QVariantMap& columns = QVariantMap()) const;
	bool updateRecord(const VectorPair& conditions, const QVariantMap& columns) const;
	bool getRecord(const VectorPair& conditions, QVariantMap& results, const QStringList& tColumns = QStringList(), const QStringList& tOrder = QStringList()) const;
	bool getRecords(QVector<QVariantMap>& results, const QStringList& tColumns = QStringList(), const QStringList& tOrder = QStringList()) const;
	bool deleteRecord(const VectorPair& conditions) const;
	bool tableExists(const QString& table) const;
	bool deleteTable(const QString& table) const;
	void setReadonlyMode(bool readOnly);

	const QJsonObject getBackup();
	QString restoreBackup(const QJsonObject& backupData);
	QString createLocalBackup();

protected:
	DBManager();
	Logger* _log;

private:	
	QString _table;

	bool _readonlyMode;
	void doAddBindValue(QSqlQuery& query, const QVariantList& variants) const;

	static QFileInfo _databaseName;
	static QThreadStorage<QSqlDatabase> _databasePool;
};
