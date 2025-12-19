#include "commandline/Parser.h"

using namespace commandline;

Option::Option(const QString& name, const QString& description, const QString& valueName, const QString& defaultValue)
	: QCommandLineOption(name, description, valueName, defaultValue)
{}

Option::Option(const QStringList& names, const QString& description, const QString& valueName, const QString& defaultValue)
	: QCommandLineOption(names, description, valueName, defaultValue)
{}

bool Option::validate(Parser& /*parser*/, QString& /*value*/)
{
	/* By default everything is accepted */
	return true;
}

Option::Option(const QCommandLineOption& other)
	: QCommandLineOption(other)
{}

Option::~Option()
= default;

QString Option::value(Parser& parser) const
{
	return parser.value(*this);
}

QString Option::name() const
{
	const QStringList n = this->names();
	return n.last();
}

QString Option::getError() const
{
	return this->_error;
}

