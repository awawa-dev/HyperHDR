#include <led-drivers/other/DriverOtherFile.h>

#include <QTextStream>
#include <QFile>

#include <linalg.h>

DriverOtherFile::DriverOtherFile(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _lastWriteTimeNano(std::chrono::high_resolution_clock::now())
	, _file(nullptr)
	, _printTimeStamp(true)
	, _infinityResolution(false)
{	
}

bool DriverOtherFile::init(QJsonObject deviceConfig)
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

	_printTimeStamp = deviceConfig["printTimeStamp"].toBool(true);

	if (_file == nullptr)
	{
		_file = new QFile(_fileName, this);
	}

	Debug(_log, "Output filename: %s", QSTRING_CSTR(_fileName));

	_infinityResolution = deviceConfig["infinityResolution"].toBool(true);

	Debug(_log, "Infinite color engine resolution: %s", (_infinityResolution) ? "true": "false");

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

int DriverOtherFile::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	return writeColors(&ledValues, nullptr);
}

std::pair<bool, int> DriverOtherFile::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (_infinityResolution)
		return { true, writeColors(nullptr, nonlinearRgbColors) };
	else
		return { false,0 };
}

int DriverOtherFile::writeColors(const std::vector<ColorRgb>* ledValues, const SharedOutputColors nonlinearRgbColors)
{	
	QTextStream out(_file);
	size_t result = 0;

	if (_printTimeStamp)
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto nanoDouble = std::chrono::duration_cast<std::chrono::nanoseconds>(now - _lastWriteTimeNano).count() / 1000000.0;

		out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " | +" << QString("%1").arg(nanoDouble, 0, 'f', 1);

		_lastWriteTimeNano = now;
	}

	out << " [";
	if (ledValues)
	{
		for (const auto& color : *ledValues)
		{
			out << "{" << color.red << "," << color.green << "," << color.blue << "}";
		}
		result = ledValues->size();
	}
	else if (nonlinearRgbColors != nullptr)
	{
		QString separator = "";
		out.setRealNumberNotation(QTextStream::FixedNotation);
		out.setRealNumberPrecision(4);

		for (auto& color : *nonlinearRgbColors)
		{
			auto format = [](float value){return QString("%1").arg(value, 8, 'f', 4, ' '); };
			out << std::exchange(separator,", ") << "{" << format(color.x * 255.f) << "," << format(color.y * 255.f)  << "," << format(color.z * 255.f) << "}";
		}
		result = nonlinearRgbColors->size();
	}

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
	out << "]" << Qt::endl;
#else
	out << "]" << endl;
#endif
	out.flush();

	return static_cast<int>(result);
}

LedDevice* DriverOtherFile::construct(const QJsonObject& deviceConfig)
{
	return new DriverOtherFile(deviceConfig);
}

bool DriverOtherFile::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("file", "leds_group_5_debug", DriverOtherFile::construct);
