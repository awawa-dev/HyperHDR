#pragma once

#include "Option.h"

namespace commandline
{

	class BooleanOption : public Option
	{
	public:
		BooleanOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString()
		);

		BooleanOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString()
		);

		BooleanOption(const QCommandLineOption& other);
	};

}
