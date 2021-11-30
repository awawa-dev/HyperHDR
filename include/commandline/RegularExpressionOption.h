#pragma once

#include "ValidatorOption.h"

namespace commandline
{

	class RegularExpressionOption : public ValidatorOption
	{
	public:
		RegularExpressionOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString());

		RegularExpressionOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString());

		RegularExpressionOption(const QCommandLineOption& other);

		RegularExpressionOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString(),
			const QRegularExpression& expression = QRegularExpression());

		RegularExpressionOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString(),
			const QRegularExpression& expression = QRegularExpression());

		RegularExpressionOption(const QCommandLineOption& other,
			const QRegularExpression& expression = QRegularExpression());

		RegularExpressionOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString(),
			const QString& expression = QString());

		RegularExpressionOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString(),
			const QString& expression = QString());

		RegularExpressionOption(const QCommandLineOption& other,
			const QString& expression = QString());
	};

}
