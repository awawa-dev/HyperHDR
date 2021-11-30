#include "commandline/Parser.h"

using namespace commandline;


IntOption::IntOption(const QString& name,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	int minimum,
	int maximum)
	: ValidatorOption(name, description, valueName, defaultValue)
{
	setValidator(new QIntValidator(minimum, maximum));
}

IntOption::IntOption(const QStringList& names,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	int minimum,
	int maximum)
	: ValidatorOption(names, description, valueName, defaultValue)
{
	setValidator(new QIntValidator(minimum, maximum));
}

IntOption::IntOption(const QCommandLineOption& other,
	int minimum,
	int maximum)
	: ValidatorOption(other)
{
	setValidator(new QIntValidator(minimum, maximum));
}

int IntOption::getInt(Parser& parser, bool* ok, int base)
{
	_int = value(parser).toInt(ok, base);
	return _int;
}

int* IntOption::getIntPtr(Parser& parser, bool* ok, int base)
{
	if (parser.isSet(this))
	{
		getInt(parser, ok, base);
		return &_int;
	}
	return nullptr;
}
