#pragma once

#ifndef PCH_ENABLED
	#include <QFile>
	#include <QJsonDocument>
	#include <QRegularExpression>
	#include <QString>
	#include <QStringList>
	#include <QJsonObject>
	#include <QJsonValue>
	#include <QJsonArray>

	#include <iostream>
	#include <stdexcept>
#endif

#include <json-utils/jsonschema/QJsonSchemaChecker.h>

class QJsonUtils
{
public:
	static int load(const QString& schema, const QString& config, QJsonObject& json)
	{
		QJsonObject schemaTree = readSchema(schema);
		QJsonObject configTree = readConfig(config);

		QJsonSchemaChecker schemaChecker;
		schemaChecker.setSchema(schemaTree);

		QStringList messages = schemaChecker.getMessages();

		if (!schemaChecker.validate(configTree).first)
		{
			for (int i = 0; i < messages.size(); ++i)
				std::cout << messages[i].toStdString() << std::endl;

			std::cerr << "Validation failed for configuration file: " << config.toStdString() << std::endl;
			return -3;
		}

		json = configTree;
		return 0;
	}

	static QJsonObject readConfig(const QString& path)
	{
		QFile file(path);
		QJsonParseError error;

		if (!file.open(QIODevice::ReadOnly))
		{
			throw std::runtime_error(QString("Configuration file not found: '" + path + "' (" + file.errorString() + ")").toStdString());
		}

		//Allow Comments in Config
		QString config = QString(file.readAll());
		config.remove(QRegularExpression("([^:]?\\/\\/.*)"));

		QJsonDocument doc = QJsonDocument::fromJson(config.toUtf8(), &error);
		file.close();

		if (error.error != QJsonParseError::NoError)
		{
			// report to the user the failure and their locations in the document.
			int errorLine(0), errorColumn(0);

			for (int i = 0, count = qMin(error.offset, config.size()); i < count; ++i)
			{
				++errorColumn;
				if (config.at(i) == '\n')
				{
					errorColumn = 0;
					++errorLine;
				}
			}

			throw std::runtime_error(
				QString("Failed to parse configuration: " + error.errorString() + " at Line: " + QString::number(errorLine) + ", Column: " + QString::number(errorColumn)).toStdString()
			);
		}

		return doc.object();
	}

	static QJsonObject readSchema(const QString& path)
	{
		QFile schemaData(path);
		QJsonParseError error;

		if (!schemaData.open(QIODevice::ReadOnly))
		{
			throw std::runtime_error(QString("Schema not found: '" + path + "' (" + schemaData.errorString() + ")").toStdString());
		}

		QByteArray schema = schemaData.readAll();
		QJsonDocument doc = QJsonDocument::fromJson(schema, &error);
		schemaData.close();

		if (error.error != QJsonParseError::NoError)
		{
			// report to the user the failure and their locations in the document.
			int errorLine(0), errorColumn(0);

			for (int i = 0, count = qMin(error.offset, schema.size()); i < count; ++i)
			{
				++errorColumn;
				if (schema.at(i) == '\n')
				{
					errorColumn = 0;
					++errorLine;
				}
			}

			throw std::runtime_error(QString("ERROR: Json schema wrong: " + error.errorString() +
				" at Line: " + QString::number(errorLine) +
				", Column: " + QString::number(errorColumn)).toStdString());
		}

		return resolveReferences(doc.object());
	}

	static QJsonObject resolveReferences(const QJsonObject& schema)
	{
		QJsonObject result;

		for (QJsonObject::const_iterator i = schema.begin(); i != schema.end(); ++i)
		{
			QString attribute = i.key();
			const QJsonValue& attributeValue = *i;

			if (attribute == "$ref" && attributeValue.isString())
			{
				try
				{
					result = readSchema(":/" + attributeValue.toString());
				}
				catch (std::runtime_error& error)
				{
					throw std::runtime_error(error.what());
				}
			}
			else if (attributeValue.isObject())
				result.insert(attribute, resolveReferences(attributeValue.toObject()));
			else
				result.insert(attribute, attributeValue);
		}

		return result;
	}

	static bool writeJson(const QString& filename, QJsonObject& jsonTree)
	{
		QJsonDocument doc;

		doc.setObject(jsonTree);
		QByteArray configData = doc.toJson(QJsonDocument::Indented);

		QFile configFile(filename);
		if (!configFile.open(QFile::WriteOnly | QFile::Truncate))
			return false;

		configFile.write(configData);

		QFile::FileError error = configFile.error();
		if (error != QFile::NoError)
			return false;

		configFile.close();

		return true;
	}

	static void modify(QJsonObject& value, QStringList path, const QJsonValue& newValue = QJsonValue::Null, QString propertyName = "")
	{
		QJsonObject result;

		if (!path.isEmpty())
		{
			if (path.first() == "[root]")
				path.removeFirst();

			for (QStringList::iterator it = path.begin(); it != path.end(); ++it)
			{
				QString current = *it;
				if (current.left(1) == ".")
					*it = current.mid(1, current.size() - 1);
			}

			if (!value.isEmpty())
				modifyValue(value, result, path, newValue, propertyName);
			else if (newValue != QJsonValue::Null && !propertyName.isEmpty())
				result[propertyName] = newValue;
		}

		value = result;
	}

	static QJsonValue create(QJsonValue schema, bool ignoreRequired = false)
	{
		return createValue(schema, ignoreRequired);
	}

	static QString outputNode(const QJsonValue& value)
	{
		switch (value.type())
		{
		case QJsonValue::Array:
		{
			QString ret = "[";
			for (const QJsonValueRef v : value.toArray())
			{
				ret += ((ret.length() > 1) ? ", ": "") + outputNode(v);
			}
			return ret + "]";
			break;
		}
		case QJsonValue::Object:
		{
			return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
			break;
		}
		case QJsonValue::Bool:
			return value.toBool() ? "True" : "False";
		case QJsonValue::Double:
			return QString::number(value.toDouble());
		case QJsonValue::String:
			return value.toString();
		case QJsonValue::Null:
		case QJsonValue::Undefined:
			break;
		}
		return "???";
	}

private:

	static QJsonValue createValue(QJsonValue schema, bool ignoreRequired)
	{
		QJsonObject result;
		QJsonObject obj = schema.toObject();

		if (obj.find("type") != obj.end() && obj.find("type").value().isString())
		{
			QJsonValue ret = QJsonValue::Null;

			if (obj.find("type").value().toString() == "object" && (obj.find("required").value().toBool() || ignoreRequired))
				ret = createValue(obj["properties"], ignoreRequired);
			else if (obj.find("type").value().toString() == "array" && (obj.find("required").value().toBool() || ignoreRequired))
			{
				QJsonArray array;

				if (obj.find("default") != obj.end())
					ret = obj.find("default").value();
				else
				{
					ret = createValue(obj["items"], ignoreRequired);

					if (!ret.toObject().isEmpty())
						array.append(ret);
					ret = array;
				}
			}
			else if (obj.find("required").value().toBool() || ignoreRequired)
				if (obj.find("default") != obj.end())
					ret = obj.find("default").value();

			return ret;
		}
		else
		{
			for (QJsonObject::const_iterator i = obj.begin(); i != obj.end(); ++i)
			{
				QString attribute = i.key();
				const QJsonObject& attributeValue = i.value().toObject();
				QJsonValue subValue = obj[attribute];

				if (attributeValue.find("type") != attributeValue.end())
				{
					if (attributeValue.find("type").value().toString() == "object" && (attributeValue.find("required").value().toBool() || ignoreRequired))
					{
						if (obj.contains("properties"))
							result[attribute] = createValue(obj["properties"], ignoreRequired);
						else
							result[attribute] = createValue(subValue, ignoreRequired);
					}
					else if (attributeValue.find("type").value().toString() == "array" && (attributeValue.find("required").value().toBool() || ignoreRequired))
					{
						QJsonArray array;

						if (attributeValue.find("default") != attributeValue.end())
							result[attribute] = attributeValue.find("default").value();
						else
						{
							QJsonValue retEmpty;
							retEmpty = createValue(attributeValue["items"], ignoreRequired);

							if (!retEmpty.toObject().isEmpty())
								array.append(retEmpty);
							result[attribute] = array;
						}
					}
					else if ((attributeValue.find("required") != attributeValue.end() && attributeValue.find("required").value().toBool()) || ignoreRequired)
					{
						if (attributeValue.find("default") != attributeValue.end())
							result[attribute] = attributeValue.find("default").value();
						else
							result[attribute] = QJsonValue::Null;
					}
				}
			}
		}

		return result;
	}

	static void modifyValue(QJsonValue source, QJsonObject& target, QStringList path, const QJsonValue& newValue, QString& property)
	{
		QJsonObject obj = source.toObject();

		if (!obj.isEmpty())
		{
			for (QJsonObject::iterator i = obj.begin(); i != obj.end(); ++i)
			{
				QString propertyName = i.key();
				QJsonValue subValue = obj[propertyName];

				if (subValue.isObject())
				{
					if (!path.isEmpty())
					{
						if (propertyName == path.first())
						{
							path.removeFirst();

							if (!path.isEmpty())
							{
								QJsonObject obj;
								modifyValue(subValue, obj, path, newValue, property);
								subValue = obj;
							}
							else if (newValue != QJsonValue::Null)
								subValue = newValue;
							else
								continue;

							if (!subValue.toObject().isEmpty())
								target[propertyName] = subValue;
						}
						else
						{
							if (path.first() == property && newValue != QJsonValue::Null)
							{
								target[property] = newValue;
								property = QString();
							}

							target[propertyName] = subValue;
						}
					}
					else
						if (!subValue.toObject().isEmpty())
							target[propertyName] = subValue;
				}
				else if (subValue.isArray())
				{
					if (!path.isEmpty())
					{
						if (propertyName == path.first())
						{
							path.removeFirst();

							int arrayLevel = -1;
							if (!path.isEmpty())
							{
								if ((path.first().left(1) == "[") && (path.first().right(1) == "]"))
								{
									arrayLevel = path.first().mid(1, path.first().size() - 2).toInt();
									path.removeFirst();
								}
							}

							QJsonArray array = subValue.toArray();
							QJsonArray json_array;

							for (QJsonArray::iterator i = array.begin(); i != array.end(); ++i)
							{
								if (!path.isEmpty())
								{
									QJsonObject arr;
									modifyValue(*i, arr, path, newValue, property);
									subValue = arr;
								}
								else if (newValue != QJsonValue::Null)
									subValue = newValue;
								else
									continue;

								if (!subValue.toObject().isEmpty())
									json_array.append(subValue);
								else if (newValue != QJsonValue::Null && arrayLevel != -1)
									json_array.append((i - array.begin() == arrayLevel) ? subValue : *i);
							}

							if (!json_array.isEmpty())
								target[propertyName] = json_array;
							else if (newValue != QJsonValue::Null && arrayLevel == -1)
								target[propertyName] = newValue;
						}
						else
						{
							if (path.first() == property && newValue != QJsonValue::Null)
							{
								target[property] = newValue;
								property = QString();
							}

							target[propertyName] = subValue;
						}
					}
					else
						if (!subValue.toArray().isEmpty())
							target[propertyName] = subValue;
				}
				else
				{
					if (!path.isEmpty())
					{
						if (propertyName == path.first())
						{
							path.removeFirst();

							if (path.isEmpty())
							{
								if (newValue != QJsonValue::Null && property.isEmpty())
									subValue = newValue;
								else
									continue;
							}

							target[propertyName] = subValue;
						}
						else
						{
							if (path.first() == property && newValue != QJsonValue::Null)
							{
								target[property] = newValue;
								property = QString();
							}

							target[propertyName] = subValue;
						}
					}
					else
						target[propertyName] = subValue;
				}
			}
		}
		else if (newValue != QJsonValue::Null && !property.isEmpty())
		{
			target[property] = newValue;
			property = QString();
		}
	}
};
