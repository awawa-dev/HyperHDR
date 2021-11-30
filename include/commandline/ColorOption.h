#pragma once

#include "Option.h"

namespace commandline
{

	class ColorOption : public Option
	{
	protected:
		QColor _color;

	public:
		ColorOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString()
		);

		ColorOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString()
		);

		ColorOption(const QCommandLineOption& other);

		bool validate(Parser& parser, QString& value) override;
		QColor getColor(Parser& parser) const;
	};

}


