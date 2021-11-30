#include "commandline/Parser.h"

using namespace commandline;

ColorOption::ColorOption(const QString& name,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue
)
	: Option(name, description, valueName, defaultValue)
{}

ColorOption::ColorOption(const QStringList& names,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue
)
	: Option(names, description, valueName, defaultValue)
{}

ColorOption::ColorOption(const QCommandLineOption& other)
	: Option(other)
{}

QColor ColorOption::getColor(Parser& parser) const
{
	return _color;
}

bool ColorOption::validate(Parser& parser, QString& value)
{
	// Check if we can create the color by name
	_color = QColor(value);
	if (_color.isValid())
	{
		return true;
	}

	// check if we can create the color by hex RRGGBB getColors
	_color = QColor(QString("#%1").arg(value));
	if (_color.isValid())
	{
		return true;
	}

	if (!parser.isSet(*this))
	{
		// Return true if no value is available
		return true;
	}

	_error = QString("Invalid color. A color is specified by a six lettered RRGGBB hex getColors or one of the following names:\n\t- %1").arg(QColor::colorNames().join("\n\t- "));

	return false;
}
