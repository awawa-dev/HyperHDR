#pragma once

#include "Option.h"

namespace commandline
{

	class ColorsOption : public Option
	{
	protected:
		QList<QColor> _colors;

	public:
		ColorsOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString()
		);

		ColorsOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString()
		);

		ColorsOption(const QCommandLineOption& other);

		virtual bool validate(Parser& parser, QString& value) override;

		QList<QColor> getColors(Parser& parser) const;
	};

}


