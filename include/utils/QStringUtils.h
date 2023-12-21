#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QStringList>
	#include <QVector>
#endif

namespace QStringUtils {

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
	inline QStringList SPLITTER(const QString& string, QChar sep)
	{
		return string.split(sep, Qt::SkipEmptyParts);
	}
#else
	inline QStringList SPLITTER(const QString& string, QChar sep)
	{
		return string.split(sep, QString::SkipEmptyParts);
	}
#endif
}
