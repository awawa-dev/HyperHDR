#include <QCryptographicHash>
#include <QNetworkInterface>

#include <db/MetaTable.h>

MetaTable::MetaTable()
	: DBManager()
{
	setTable("meta");
	createTable(QStringList() << "uuid TEXT" << "created_at TEXT");
};

QString  MetaTable::getUUID() const
{
	QVector<QVariantMap> results;
	getRecords(results, QStringList() << "uuid");

	for (const auto& entry : results)
	{
		if (!entry["uuid"].toString().isEmpty())
			return entry["uuid"].toString();
	}

	// create new uuidv5 based on net adapter MAC, save to db and return
	QString hash;
	foreach(QNetworkInterface interface, QNetworkInterface::allInterfaces())
	{
		if (!(interface.flags() & QNetworkInterface::IsLoopBack))
		{
			hash = QCryptographicHash::hash(interface.hardwareAddress().toLocal8Bit(), QCryptographicHash::Sha1).toHex();
			break;
		}
	}
	const QString newUuid = QUuid::createUuidV5(QUuid(), hash).toString().mid(1, 36);
	VectorPair cond;
	cond.append(CPair("uuid", newUuid));
	QVariantMap map;
	map["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	createRecord(cond, map);

	return newUuid;
}

