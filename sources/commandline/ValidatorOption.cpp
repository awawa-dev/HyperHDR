#include "commandline/Parser.h"

using namespace commandline;

ValidatorOption::ValidatorOption(const QString& name,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	const QValidator* validator)
	: Option(name, description, valueName, defaultValue), validator(validator)
{}

ValidatorOption::ValidatorOption(const QStringList& names,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	const QValidator* validator)
	: Option(names, description, valueName, defaultValue), validator(validator)
{}

ValidatorOption::ValidatorOption(const QCommandLineOption& other,
	const QValidator* validator)
	: Option(other), validator(validator)
{}

bool ValidatorOption::validate(Parser& parser, QString& value)
{
	if (parser.isSet(*this) || !defaultValues().empty())
	{
		int pos = 0;
		validator->fixup(value);
		return validator->validate(value, pos) == QValidator::Acceptable;
	}
	return true;
}

const QValidator* ValidatorOption::getValidator() const
{
	return validator;
}

void ValidatorOption::setValidator(const QValidator* validator)
{
	ValidatorOption::validator = validator;
}

