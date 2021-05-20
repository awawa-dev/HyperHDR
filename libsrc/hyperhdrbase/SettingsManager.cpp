// proj
#include <hyperhdrbase/SettingsManager.h>

// util
#include <utils/JsonUtils.h>
#include <db/SettingsTable.h>

// json schema process
#include <utils/jsonschema/QJsonFactory.h>
#include <utils/jsonschema/QJsonSchemaChecker.h>

// write config to filesystem
#include <utils/JsonUtils.h>

QJsonObject SettingsManager::schemaJson;

SettingsManager::SettingsManager(quint8 instance, QObject* parent, bool readonlyMode)
	: QObject(parent)
	, _log(Logger::getInstance("SETTINGSMGR"))
	, _sTable(new SettingsTable(instance, this))
	, _readonlyMode(readonlyMode)
{
	_sTable->setReadonlyMode(_readonlyMode);	

	// get schema
	if(schemaJson.isEmpty())
	{
		Q_INIT_RESOURCE(resource);
		try
		{
			schemaJson = QJsonFactory::readSchema(":/hyperhdr-schema");
		}
		catch(const std::runtime_error& error)
		{
			throw std::runtime_error(error.what());
		}
	}

	// get default config
	QJsonObject defaultConfig;	
	for (settings::type selector = settings::type::SNDEFFECT; selector != settings::type::INVALID; selector = settings::type(((int)selector) + 1))
	{
		if (selector == settings::type::VIDEOGRABBER)
		{			
			if(!_sTable->recordExist(typeToString(selector)) && _sTable->recordExist("grabberV4L2"))
			{
				auto oldVideo = _sTable->getSettingsRecord("grabberV4L2").object();

				oldVideo["device"] = oldVideo["available_devices"];

				oldVideo["videoEncoding"] = oldVideo["v4l2Encoding"];
				oldVideo["videoMode"] = QString("%1x%2").arg(oldVideo["width"].toInt()).arg(oldVideo["height"].toInt());

				defaultConfig.insert(typeToString(selector), oldVideo);
				_sTable->deleteSettingsRecordString("grabberV4L2");

				Warning(_log, "Found old V4L2 configuration. Migrating defaults to the new structures.");				
				continue;
			}
		}
	
		if (selector != settings::type::LEDS)
			defaultConfig.insert(typeToString(selector), QJsonObject());
		else
			defaultConfig.insert(typeToString(selector), QJsonArray());
	}	

	// transform json to string lists
	QStringList keyList = defaultConfig.keys();
	QStringList defValueList;
	for(const auto & key : keyList)
	{
		if(defaultConfig[key].isObject())
		{
			defValueList << QString(QJsonDocument(defaultConfig[key].toObject()).toJson(QJsonDocument::Compact));
		}
		else if(defaultConfig[key].isArray())
		{
			defValueList << QString(QJsonDocument(defaultConfig[key].toArray()).toJson(QJsonDocument::Compact));
		}
	}

	// fill database with default data if required
	for(const auto & key : keyList)
	{
		QString val = defValueList.takeFirst();
		// prevent overwrite
		if(!_sTable->recordExist(key))
			_sTable->createSettingsRecord(key,val);
	}

	// need to validate all data in database constuct the entire data object
	// TODO refactor schemaChecker to accept QJsonArray in validate(); QJsonDocument container? To validate them per entry...
	QJsonObject dbConfig;
	for(const auto & key : keyList)
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
	schemaChecker.setSchema(schemaJson);
	QPair<bool,bool> valid = schemaChecker.validate(dbConfig);
	// check if our main schema syntax is IO
	if (!valid.second)
	{
		for (auto & schemaError : schemaChecker.getMessages())
			Error(_log, "Schema Syntax Error: %s", QSTRING_CSTR(schemaError));
		throw std::runtime_error("The config schema has invalid syntax. This should never happen! Go fix it!");
	}
	if (!valid.first)
	{
		Info(_log,"Table upgrade required...");
		dbConfig = schemaChecker.getAutoCorrectedConfig(dbConfig);

		for (auto & schemaError : schemaChecker.getMessages())
			Warning(_log, "Config Fix: %s", QSTRING_CSTR(schemaError));

		saveSettings(dbConfig,true);
	}
	else
		_qconfig = dbConfig;

	Info(_log,"Settings database initialized");
}

QJsonDocument SettingsManager::getSetting(settings::type type) const
{
	return _sTable->getSettingsRecord(settings::typeToString(type));
}

QJsonObject SettingsManager::getSettings() const
{	
	QJsonObject dbConfig;
	for(const auto & key : _qconfig.keys())
	{
		QJsonDocument doc = _sTable->getSettingsRecord(key);
		if(doc.isArray())
			dbConfig[key] = doc.array();
		else
			dbConfig[key] = doc.object();
	}
	return dbConfig;
}

bool SettingsManager::saveSettings(QJsonObject config, bool correct)
{
	// optional data upgrades e.g. imported legacy/older configs
	// handleConfigUpgrade(config);

	// we need to validate data against schema
	QJsonSchemaChecker schemaChecker;
	schemaChecker.setSchema(schemaJson);
	if (!schemaChecker.validate(config).first)
	{
		if(!correct)
		{
			Error(_log,"Failed to save configuration, errors during validation");
			return false;
		}
		Warning(_log,"Fixing json data!");
		config = schemaChecker.getAutoCorrectedConfig(config);

		for (const auto & schemaError : schemaChecker.getMessages())
			Warning(_log, "Config Fix: %s", QSTRING_CSTR(schemaError));
	}

	// store the new config
	_qconfig = config;

	// extract keys and data
	QStringList keyList = config.keys();
	QStringList newValueList;
	for(const auto & key : keyList)
	{
		if(config[key].isObject())
		{
			newValueList << QString(QJsonDocument(config[key].toObject()).toJson(QJsonDocument::Compact));
		}
		else if(config[key].isArray())
		{
			newValueList << QString(QJsonDocument(config[key].toArray()).toJson(QJsonDocument::Compact));
		}
	}

	int rc = true;
	// compare database data with new data to emit/save changes accordingly
	for(const auto & key : keyList)
	{
		QString data = newValueList.takeFirst();
		QString recordData = _sTable->getSettingsRecordString(key);
		if ((recordData != data) &&
			(settings::stringToType(key) != settings::type::VIDEODETECTION || recordData.length() < 8))
		{
			if ( ! _sTable->createSettingsRecord(key, data) )
			{
				rc = false;
			}
			else
			{
				emit settingsChanged(settings::stringToType(key), QJsonDocument::fromJson(data.toUtf8()));
			}
		}
	}	
	return rc;
}

void SettingsManager::saveSetting(settings::type key, QString saveData)
{
	if (!_sTable->createSettingsRecord(settings::typeToString(key), saveData))
	{
		Error(_log, "Could not save the config: %s = %s", QSTRING_CSTR(settings::typeToString(key)), QSTRING_CSTR(saveData));
	}
	else
	{
		emit settingsChanged(key, QJsonDocument::fromJson(saveData.toUtf8()));
	}
}
