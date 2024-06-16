#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>	
#endif

#include <utils/Logger.h>
#include <utils/FileUtils.h>

namespace JsonUtils {
	bool readFile(const QString& path, QJsonObject& obj, Logger* log, bool ignError = false);
	bool readSchema(const QString& path, QJsonObject& obj, Logger* log);
	bool parse(const QString& path, const QString& data, QJsonObject& obj, Logger* log);
	bool parse(const QString& path, const QString& data, QJsonArray& arr, Logger* log);
	bool parse(const QString& path, const QString& data, QJsonDocument& doc, Logger* log);
	bool validate(const QString& file, const QJsonObject& json, const QString& schemaPath, Logger* log);
	bool validate(const QString& file, const QJsonObject& json, const QJsonObject& schema, Logger* log);
	bool write(const QString& filename, const QJsonObject& json, Logger* log);
	bool resolveRefs(const QJsonObject& schema, QJsonObject& obj, Logger* log);
}
