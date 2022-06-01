#include <db/SettingsTable.h>
#include <utils/settings.h>

#define INSTANCE_COLUMN QString("hyperhdr_instance")

SettingsTable::SettingsTable(quint8 instance, QObject* parent)
	: DBManager(parent)
	, _hyperhdr_inst(instance)
{
	setTable("settings");
	// create table columns
	createTable(QStringList() << "type TEXT" << "config TEXT" << (INSTANCE_COLUMN + " INTEGER") << "updated_at TEXT");

};

///
/// @brief      Create or update a settings record
/// @param[in]  type           type of setting
/// @param[in]  config         The configuration data
/// @return     true on success else false
///
bool SettingsTable::createSettingsRecord(const QString& type, const QString& config) const
{
	QVariantMap map;
	map["config"] = config;
	map["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	VectorPair cond;
	cond.append(CPair("type", type));
	// when a setting is not global we are searching also for the instance
	if (!isSettingGlobal(type))
		cond.append(CPair("AND " + INSTANCE_COLUMN, _hyperhdr_inst));
	return createRecord(cond, map);
}

///
/// @brief      Test if record exist, type can be global setting or local (instance)
/// @param[in]  type           type of setting
/// @param[in]  hyperhdr_instance  The instance of hyperhdr assigned (might be empty)
/// @return     true on success else false
///
bool SettingsTable::recordExist(const QString& type) const
{
	VectorPair cond;
	cond.append(CPair("type", type));
	// when a setting is not global we are searching also for the instance
	if (!isSettingGlobal(type))
		cond.append(CPair("AND " + INSTANCE_COLUMN, _hyperhdr_inst));
	return recordExists(cond);
}

///
/// @brief Get 'config' column of settings entry as QJsonDocument
/// @param[in]  type   The settings type
/// @return            The QJsonDocument
///
QJsonDocument SettingsTable::getSettingsRecord(const QString& type) const
{
	QVariantMap results;
	VectorPair cond;
	cond.append(CPair("type", type));
	// when a setting is not global we are searching also for the instance
	if (!isSettingGlobal(type))
		cond.append(CPair("AND " + INSTANCE_COLUMN, _hyperhdr_inst));
	getRecord(cond, results, QStringList("config"));
	return QJsonDocument::fromJson(results["config"].toByteArray());
}

///
/// @brief Get 'config' column of settings entry as QString
/// @param[in]  type   The settings type
/// @return            The QString
///
QString SettingsTable::getSettingsRecordString(const QString& type) const
{
	QVariantMap results;
	VectorPair cond;
	cond.append(CPair("type", type));
	// when a setting is not global we are searching also for the instance
	if (!isSettingGlobal(type))
		cond.append(CPair("AND " + INSTANCE_COLUMN, _hyperhdr_inst));
	getRecord(cond, results, QStringList("config"));
	return results["config"].toString();
}

bool SettingsTable::deleteSettingsRecordString(const QString& type) const
{
	QVariantMap results;
	VectorPair cond;
	cond.append(CPair("type", type));
	// when a setting is not global we are searching also for the instance
	if (!isSettingGlobal(type))
		cond.append(CPair("AND " + INSTANCE_COLUMN, _hyperhdr_inst));
	return deleteRecord(cond);
}

bool SettingsTable::purge(const QString& type) const
{
	QVariantMap results;
	VectorPair cond;
	cond.append(CPair("type", type));
	// when a setting is not global we are searching also for the instance
	return deleteRecord(cond);
}
///
/// @brief Delete all settings entries associated with this instance, called from InstanceTable of HyperHDRIManager
///
void SettingsTable::deleteInstance() const
{
	VectorPair cond;
	cond.append(CPair(INSTANCE_COLUMN, _hyperhdr_inst));
	deleteRecord(cond);
}

bool SettingsTable::isSettingGlobal(const QString& type) const
{
	// list of global settings
	QStringList list;
	// server port services
	list << settings::typeToString(settings::type::JSONSERVER) << settings::typeToString(settings::type::PROTOSERVER)
		<< settings::typeToString(settings::type::FLATBUFSERVER) << settings::typeToString(settings::type::NETWORK)
		<< settings::typeToString(settings::type::NETFORWARD) << settings::typeToString(settings::type::WEBSERVER)
		<< settings::typeToString(settings::type::VIDEOGRABBER) << settings::typeToString(settings::type::SYSTEMGRABBER)
		<< settings::typeToString(settings::type::LOGGER) << settings::typeToString(settings::type::GENERAL)
		<< settings::typeToString(settings::type::SNDEFFECT) << settings::typeToString(settings::type::VIDEODETECTION)
		<< settings::typeToString(settings::type::MQTT)
		// legacy
		<< "grabberV4L2";

	return list.contains(type);
}
