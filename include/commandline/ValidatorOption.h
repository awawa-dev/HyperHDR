#pragma once

#include "Option.h"

namespace commandline
{

	class ValidatorOption : public Option
	{
	protected:
		const QValidator* validator;
		virtual void setValidator(const QValidator* validator);

	public:
		ValidatorOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString(),
			const QValidator* validator = nullptr);

		ValidatorOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString(),
			const QValidator* validator = nullptr);

		ValidatorOption(const QCommandLineOption& other,
			const QValidator* validator = nullptr);

		virtual const QValidator* getValidator() const;
		virtual bool validate(Parser& parser, QString& value) override;
	};

}

