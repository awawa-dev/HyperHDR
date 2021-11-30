#pragma once

#include <limits>
#include <QtCore>
#include "ValidatorOption.h"

namespace commandline
{

	class IntOption : public ValidatorOption
	{
	protected:
		int _int = 0;

	public:
		IntOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString(),
			int minimum = std::numeric_limits<int>::min(), int maximum = std::numeric_limits<int>::max());

		IntOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString(),
			int minimum = std::numeric_limits<int>::min(), int maximum = std::numeric_limits<int>::max());

		IntOption(const QCommandLineOption& other,
			int minimum = std::numeric_limits<int>::min(), int maximum = std::numeric_limits<int>::max());

		int getInt(Parser& parser, bool* ok = 0, int base = 10);
		int* getIntPtr(Parser& parser, bool* ok = 0, int base = 10);
	};

}
