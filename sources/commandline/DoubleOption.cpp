#include "commandline/Parser.h"

using namespace commandline;

DoubleOption::DoubleOption(const QString& name,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	double minimum,
	double maximum,
	int decimals)
	: ValidatorOption(name, description, valueName, defaultValue)
{
	setValidator(new QDoubleValidator(minimum, maximum, decimals));
}

DoubleOption::DoubleOption(const QStringList& names,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	double minimum,
	double maximum,
	int decimals)
	: ValidatorOption(names, description, valueName, defaultValue)
{
	setValidator(new QDoubleValidator(minimum, maximum, decimals));
}

DoubleOption::DoubleOption(const QCommandLineOption& other,
	double minimum,
	double maximum,
	int decimals)
	: ValidatorOption(other)
{
	setValidator(new QDoubleValidator(minimum, maximum, decimals));
}

double DoubleOption::getDouble(Parser& parser, bool* ok)
{
	_double = value(parser).toDouble(ok);
	return _double;
}

double* DoubleOption::getDoublePtr(Parser& parser, bool* ok)
{
	if (parser.isSet(this))
	{
		getDouble(parser, ok);
		return &_double;
	}

	return nullptr;
}
