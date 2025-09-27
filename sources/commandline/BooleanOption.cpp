#include "commandline/BooleanOption.h"

using namespace commandline;

BooleanOption::BooleanOption(const QString& name,
	const QString& description,
	const QString& /*valueName*/,
	const QString& /*defaultValue*/
)
	: Option(name, description, QString(), QString())
{}

BooleanOption::BooleanOption(const QStringList& names,
	const QString& description,
	const QString& /*valueName*/,
	const QString& /*defaultValue*/
)
	: Option(names, description, QString(), QString())
{}

BooleanOption::BooleanOption(const QCommandLineOption& other)
	: Option(other)
{}
