#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
#endif

#include <utils/Logger.h>
#include <utils/FileUtils.h>

namespace JsonUtils {
	bool readFile(const QString& path, QJsonObject& obj, const LoggerName& log, bool ignError = false);
	bool readSchema(const QString& path, QJsonObject& obj, const LoggerName& log);
	bool parse(const QString& path, const QString& data, QJsonObject& obj, const LoggerName& log);
	bool parse(const QString& path, const QString& data, QJsonArray& arr, const LoggerName& log);
	bool parse(const QString& path, const QString& data, QJsonDocument& doc, const LoggerName& log);
	bool validate(const QString& file, const QJsonObject& json, const QString& schemaPath, const LoggerName& log);
	bool validate(const QString& file, const QJsonObject& json, const QJsonObject& schema, const LoggerName& log);
	bool write(const QString& filename, const QJsonObject& json, const LoggerName& log);
	bool resolveRefs(const QJsonObject& schema, QJsonObject& obj, const LoggerName& log);
}
