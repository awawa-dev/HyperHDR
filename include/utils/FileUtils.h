#pragma once

#ifndef PCH_ENABLED
	#include <QFile>
	#include <QString>
	#include <QByteArray>	
#endif

#include <utils/Logger.h>

namespace FileUtils {

	QString getBaseName(const QString& sourceFile);

	bool fileExists(const QString& path, Logger* log, bool ignError = false);
	bool readFile(const QString& path, QString& data, Logger* log, bool ignError = false);
	bool writeFile(const QString& path, const QByteArray& data, Logger* log);
	void resolveFileError(const QFile& file, Logger* log);
}
