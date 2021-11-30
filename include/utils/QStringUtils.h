#ifndef QSTRINGUTILS_H
#define QSTRINGUTILS_H

#include <QString>
#include <QStringList>
#include <QVector>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QStringView>
#else
#include <QStringRef>
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

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	inline QList<QStringView> REFSPLITTER(const QString& string, QChar sep)
	{
		return QStringView{ string }.split(sep, Qt::SplitBehaviorFlags::SkipEmptyParts);
	}
#elif (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
	inline QVector<QStringRef> REFSPLITTER(const QString& string, QChar sep)
	{
		return string.splitRef(sep, Qt::SkipEmptyParts);
	}
#else
	inline QVector<QStringRef> REFSPLITTER(const QString& string, QChar sep)
	{
		return string.splitRef(sep, QString::SkipEmptyParts);
	}
#endif

}

#endif // QSTRINGUTILS_H
