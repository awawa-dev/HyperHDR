#ifndef PCH_ENABLED
	#include <QMutexLocker>
#endif

#include <base/InstanceConfig.h>
#include <json-utils/JsonUtils.h>
#include <db/SettingsTable.h>
#include <json-utils/jsonschema/QJsonUtils.h>
#include <json-utils/jsonschema/QJsonSchemaChecker.h>
#include <json-utils/JsonUtils.h>

QJsonObject	InstanceConfig::_schemaJson;
QMutex		InstanceConfig::_lockerSettingsManager;
std::atomic<bool> InstanceConfig::_backupMade(false);

InstanceConfig::InstanceConfig(bool master, quint8 instanceIndex, QObject* parent)
	: QObject(parent)
	, _log(Logger::getInstance(QString("INSTANCE_CFG%1").arg((master) ? QString() : QString::number(instanceIndex))))
{
	QMutexLocker locker(&_lockerSettingsManager);

	Info(_log, "Loading instance configuration");

	_sTable = std::unique_ptr<SettingsTable>(new SettingsTable(instanceIndex));

	// get schema
	if (_schemaJson.isEmpty())
	{
		Q_INIT_RESOURCE(resource);
		try
		{
			_schemaJson = QJsonUtils::readSchema(":/hyperhdr-schema");
		}
		catch (const std::runtime_error& error)
		{
			throw std::runtime_error(error.what());
		}
	}

	// get default config
	QJsonObject defaultConfig;
	for (settings::type selector = settings::type::SNDEFFECT; selector != settings::type::INVALID; selector = settings::type(((int)selector) + 1))
	{
		if (selector != settings::type::LEDS)
			defaultConfig.insert(typeToString(selector), QJsonObject());
		else
			defaultConfig.insert(typeToString(selector), QJsonArray());
	}

	// transform json to string lists
	QStringList keyList = defaultConfig.keys();
	QStringList defValueList;
	for (const auto& key : keyList)
	{
		if (defaultConfig[key].isObject())
		{
			defValueList << QString(QJsonDocument(defaultConfig[key].toObject()).toJson(QJsonDocument::Compact));
		}
		else if (defaultConfig[key].isArray())
		{
			defValueList << QString(QJsonDocument(defaultConfig[key].toArray()).toJson(QJsonDocument::Compact));
		}
	}

	// fill database with default data if required
	bool hasData = false;
	for (const auto& key : keyList)
	{
		QString val = defValueList.takeFirst();
		// prevent overwrite
		if (!_sTable->recordExist(key))
			_sTable->createSettingsRecord(key, val);
		else
			hasData = true;
	}
	if (!hasData && !_backupMade)
		_backupMade = true;

	// need to validate all data in database constuct the entire data object
	// TODO refactor schemaChecker to accept QJsonArray in validate(); QJsonDocument container? To validate them per entry...
	QJsonObject dbConfig;
	for (const auto& key : keyList)
	{
		QJsonDocument doc = _sTable->getSettingsRecord(key);

		if (key == typeToString(settings::type::LEDS) && doc.isArray())
		{
			QJsonArray entries = doc.array();

			for (int i = 0; i < entries.count(); i++)
			{
				QJsonObject ledItem = entries[i].toObject();
				if (!ledItem.contains("group")) {
					ledItem["group"] = 0;
					entries[i] = ledItem;
				}
			}

			dbConfig[key] = entries;
		}
		else if (doc.isArray())
			dbConfig[key] = doc.array();
		else
			dbConfig[key] = doc.object();
	}

	// validate full dbconfig against schema, on error we need to rewrite entire table
	QJsonSchemaChecker schemaChecker;
	schemaChecker.setSchema(_schemaJson);
	QPair<bool, bool> valid = schemaChecker.validate(dbConfig);
	bool upgradeNeeded = upgradeDB(dbConfig);

	if (!valid.second)
	{
		for (auto& schemaError : schemaChecker.getMessages())
			Error(_log, "Schema Syntax Error: %s", QSTRING_CSTR(schemaError));
		throw std::runtime_error("The config schema has invalid syntax. This should never happen! Go fix it!");
	}
	if (!valid.first || upgradeNeeded)
	{
		Info(_log, "Table upgrade required...");
		if (!_backupMade.exchange(true))
		{
			_backupMade = true;
			Info(_log, "Creating DB backup first.");
			QString resultFile = _sTable->createLocalBackup();
			if (!resultFile.isEmpty())
				Info(_log, "The backup is saved as: %s", QSTRING_CSTR(resultFile));
			else
				Warning(_log, "Could not create a backup");
		}
		Info(_log, "Upgrading...");
		dbConfig = schemaChecker.getAutoCorrectedConfig(dbConfig);

		for (auto& schemaError : schemaChecker.getMessages())
			Warning(_log, "Config Fix: %s", QSTRING_CSTR(schemaError));

		saveSettings(dbConfig, true);
	}
	else
		_qconfig = dbConfig;

	Info(_log, "Settings database initialized");
}

bool InstanceConfig::upgradeDB(QJsonObject& dbConfig)
{
	auto generalObject = dbConfig["general"].toObject();
	int version = generalObject["version"].toInt(-1);
	int orgVersion = version;

	if (version < 0)
	{
		Warning(_log, "Could not read DB version");
		return false;
	}

	// HyperHDR v20 update
	if (version < 2)
	{
		auto colorObject = dbConfig["color"].toObject();
		if (colorObject.contains("channelAdjustment"))
		{
			QJsonArray adjUpdate = colorObject["channelAdjustment"].toArray(), newArray;
			for (auto iter = adjUpdate.begin(); iter != adjUpdate.end(); ++iter)
			{
				QJsonObject newObject = (iter)->toObject();
				int threshold = newObject["backlightThreshold"].toInt(0);
				if (threshold > 1)
				{
					newObject["backlightThreshold"] = threshold - 1;
				}
				newArray.append(newObject);
			}
			colorObject["channelAdjustment"] = newArray;
			dbConfig["color"] = colorObject;
			///////////////////////////////////////////////////////////
			version = 2;
			Info(_log, "DB has been upgraded to version: %i", version);
		}
	}

	if (version < 3)
	{
		if (_sTable->recordExist("boblightServer"))
		{
			_sTable->deleteSettingsRecordString("boblightServer");
		}
		version = 3;
	}

	if (version < 4)
	{
		auto deviceObject = dbConfig["device"].toObject();
		if (deviceObject["type"] == "awa_spi")
		{
			deviceObject["type"] = "hyperspi";
			dbConfig["device"] = deviceObject;
		}
		version = 4;
	}

	generalObject["version"] = version;
	dbConfig["general"] = generalObject;

	return orgVersion != version;
}

InstanceConfig::~InstanceConfig()
{
	Debug(_log, "InstanceConfig has been deleted");
}

QJsonDocument InstanceConfig::getSetting(settings::type type) const
{
	return _sTable->getSettingsRecord(settings::typeToString(type));
}

QJsonObject InstanceConfig::getSettings() const
{
	QJsonObject dbConfig;
	for (const auto& key : _qconfig.keys())
	{
		QJsonDocument doc = _sTable->getSettingsRecord(key);
		if (doc.isArray())
			dbConfig[key] = doc.array();
		else
			dbConfig[key] = doc.object();
	}
	return dbConfig;
}

bool InstanceConfig::isReadOnlyMode()
{
	return _sTable->isReadOnlyMode();
}

bool InstanceConfig::saveSettings(QJsonObject config, bool correct)
{
	QJsonSchemaChecker schemaChecker;
	schemaChecker.setSchema(_schemaJson);
	if (!schemaChecker.validate(config).first)
	{
		if (!correct)
		{
			Error(_log, "Failed to save configuration, errors during validation");
			return false;
		}
		Warning(_log, "Fixing json data!");
		config = schemaChecker.getAutoCorrectedConfig(config);

		for (const auto& schemaError : schemaChecker.getMessages())
			Warning(_log, "Config Fix: %s", QSTRING_CSTR(schemaError));
	}

	// store the new config
	_qconfig = config;

	// extract keys and data
	QStringList keyList = config.keys();
	QStringList newValueList;
	for (const auto& key : keyList)
	{
		if (config[key].isObject())
		{
			newValueList << QString(QJsonDocument(config[key].toObject()).toJson(QJsonDocument::Compact));
		}
		else if (config[key].isArray())
		{
			newValueList << QString(QJsonDocument(config[key].toArray()).toJson(QJsonDocument::Compact));
		}
	}

	int rc = true;
	// compare database data with new data to emit/save changes accordingly
	for (const auto& key : keyList)
	{
		QString data = newValueList.takeFirst();
		QString recordData = _sTable->getSettingsRecordString(key);
		if ((recordData != data) &&
			(settings::stringToType(key) != settings::type::VIDEODETECTION || recordData.length() < 8))
		{
			if (!_sTable->createSettingsRecord(key, data))
			{
				rc = false;
			}
			else
			{
				emit SignalInstanceSettingsChanged(settings::stringToType(key), QJsonDocument::fromJson(data.toUtf8()));
			}
		}
	}
	return rc;
}

void InstanceConfig::saveSetting(settings::type key, QString saveData)
{
	if (!_sTable->createSettingsRecord(settings::typeToString(key), saveData))
	{
		Error(_log, "Could not save the config: %s = %s", QSTRING_CSTR(settings::typeToString(key)), QSTRING_CSTR(saveData));
	}
	else
	{
		emit SignalInstanceSettingsChanged(key, QJsonDocument::fromJson(saveData.toUtf8()));
	}
}
