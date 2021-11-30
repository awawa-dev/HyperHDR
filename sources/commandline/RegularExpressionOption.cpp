#include "commandline/RegularExpressionOption.h"

using namespace commandline;


RegularExpressionOption::RegularExpressionOption(const QString& name,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue)
	: ValidatorOption(name, description, valueName, defaultValue)
{}

RegularExpressionOption::RegularExpressionOption(const QStringList& names,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue)
	: ValidatorOption(names, description, valueName, defaultValue)
{}

RegularExpressionOption::RegularExpressionOption(const QCommandLineOption& other)
	: ValidatorOption(other)
{}

RegularExpressionOption::RegularExpressionOption(const QString& name,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	const QRegularExpression& expression)
	: ValidatorOption(name, description, valueName, defaultValue)
{
	setValidator(new QRegularExpressionValidator(expression));
}

RegularExpressionOption::RegularExpressionOption(const QStringList& names,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	const QRegularExpression& expression)
	: ValidatorOption(names, description, valueName, defaultValue)
{
	setValidator(new QRegularExpressionValidator(expression));
}

RegularExpressionOption::RegularExpressionOption(const QCommandLineOption& other,
	const QRegularExpression& expression)
	: ValidatorOption(other)
{
	setValidator(new QRegularExpressionValidator(expression));
}

RegularExpressionOption::RegularExpressionOption(const QString& name,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	const QString& expression)
	: ValidatorOption(name, description, valueName, defaultValue)
{
	setValidator(new QRegularExpressionValidator(QRegularExpression(expression)));
}

RegularExpressionOption::RegularExpressionOption(const QStringList& names,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue,
	const QString& expression)
	: ValidatorOption(names, description, valueName, defaultValue)
{
	setValidator(new QRegularExpressionValidator(QRegularExpression(expression)));
}

RegularExpressionOption::RegularExpressionOption(const QCommandLineOption& other,
	const QString& expression)
	: ValidatorOption(other)
{
	setValidator(new QRegularExpressionValidator(QRegularExpression(expression)));
}
