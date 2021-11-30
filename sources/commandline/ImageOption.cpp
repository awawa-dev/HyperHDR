#include "commandline/ImageOption.h"

using namespace commandline;

ImageOption::ImageOption(const QString& name,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue
)
	: Option(name, description, valueName, defaultValue)
{}

ImageOption::ImageOption(const QStringList& names,
	const QString& description,
	const QString& valueName,
	const QString& defaultValue
)
	: Option(names, description, valueName, defaultValue)
{}

ImageOption::ImageOption(const QCommandLineOption& other)
	: Option(other)
{}


QImage& ImageOption::getImage(Parser& parser)
{
	return _image;
}

bool ImageOption::validate(Parser& parser, QString& value)
{
	if (value.size())
	{
		_image = QImage(value);

		if (_image.isNull())
		{
			_error = QString("File %1 could not be opened as image").arg(value);
			return false;
		}
	}

	return true;
}
