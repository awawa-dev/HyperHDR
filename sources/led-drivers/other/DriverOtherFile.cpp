#include <led-drivers/other/DriverOtherFile.h>

#include <QTextStream>
#include <QFile>

DriverOtherFile::DriverOtherFile(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _lastWriteTimeNano(std::chrono::high_resolution_clock::now())
	, _file(nullptr)
{
	_printTimeStamp = false;
}

DriverOtherFile::~DriverOtherFile()
{
	delete _file;
}

LedDevice* DriverOtherFile::construct(const QJsonObject& deviceConfig)
{
	return new DriverOtherFile(deviceConfig);
}

bool DriverOtherFile::init(const QJsonObject& deviceConfig)
{
	bool initOK = LedDevice::init(deviceConfig);

	_lastWriteTimeNano = std::chrono::high_resolution_clock::now();

	_fileName = deviceConfig["output"].toString("/dev/null");

#if _WIN32
	if (_fileName == "/dev/null" || _fileName.trimmed().length() == 0)
	{
		_fileName = "\\\\.\\NUL";
	}
#endif

	_printTimeStamp = deviceConfig["printTimeStamp"].toBool(false);

	if (_file == nullptr)
	{
		_file = new QFile(_fileName, this);
	}

	Debug(_log, "Output filename: %s", QSTRING_CSTR(_fileName));

	return initOK;
}

int DriverOtherFile::open()
{
	int retval = -1;
	_isDeviceReady = false;

	Debug(_log, "Open filename: %s", QSTRING_CSTR(_fileName));

	if (!_file->isOpen())
	{
		Debug(_log, "QIODevice::WriteOnly, %s", QSTRING_CSTR(_fileName));
		if (!_file->open(QIODevice::WriteOnly | QIODevice::Text))
		{
			QString errortext = QString("(%1) %2, file: (%3)").arg(_file->error()).arg(_file->errorString(), _fileName);
			this->setInError(errortext);
		}
		else
		{
			_isDeviceReady = true;
			retval = 0;
		}
	}
	return retval;
}

int DriverOtherFile::close()
{
	int retval = 0;

	_isDeviceReady = false;
	if (_file != nullptr)
	{
		// Test, if device requires closing
		if (_file->isOpen())
		{
			// close device physically
			Debug(_log, "File: %s", QSTRING_CSTR(_fileName));
			_file->close();
		}
	}
	return retval;
}

static inline QTextStream& operator<<(QTextStream& os, const ColorRgb& color)
{
	os << "{"
		<< static_cast<unsigned>(color.red) << ","
		<< static_cast<unsigned>(color.green) << ","
		<< static_cast<unsigned>(color.blue)
		<< "}";

	return os;
}

int DriverOtherFile::write(const std::vector<ColorRgb>& ledValues)
{
	QTextStream out(_file);

	//printLedValues (ledValues);

	if (_printTimeStamp)
	{
		QDateTime now = QDateTime::currentDateTime();
		auto nowNano = std::chrono::high_resolution_clock::now();

		auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(nowNano-_lastWriteTimeNano).count();
		double nanoDouble = (nano/1000000.0);

		out << now.toString(Qt::ISODateWithMs) << " | +" << QString("%1").arg(nanoDouble, 0, 'f', 1);

		_lastWriteTimeNano = std::chrono::high_resolution_clock::now();
	}

	out << " [";
	for (ColorRgb color : ledValues)
	{
		out << color;
	}

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
	out << "]" << Qt::endl;
#else
	out << "]" << endl;
#endif
	out.flush();

	return 0;
}

bool DriverOtherFile::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("file", "leds_group_5_debug", DriverOtherFile::construct);
