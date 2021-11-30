#pragma once

#include "Option.h"

namespace commandline
{

	class ImageOption : public Option
	{
	protected:
		QImage _image;

	public:
		ImageOption(const QString& name,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString()
		);

		ImageOption(const QStringList& names,
			const QString& description = QString(),
			const QString& valueName = QString(),
			const QString& defaultValue = QString()
		);

		ImageOption(const QCommandLineOption& other);

		bool validate(Parser& parser, QString& value) override;
		QImage& getImage(Parser& parser);
	};

}
