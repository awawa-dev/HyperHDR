#include <led-drivers/other/DriverOtherFile.h>
#include <infinite-color-engine/ColorSpace.h>

#include <QTextStream>
#include <QFile>

#include <linalg.h>

DriverOtherFile::DriverOtherFile(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _lastWriteTimeNano(std::chrono::high_resolution_clock::now())
	, _file(nullptr)
	, _printTimeStamp(true)
	, _infiniteColorEngine(false)
	, _enable_ice_rgbw(false)
	, _ice_white_temperatur{ 1.0f, 1.0f, 1.0f }
	, _ice_white_mixer_threshold(0.0f)
	, _ice_white_led_intensity(1.8f)
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

	Debug(_log, "Output filename: {:s}", (_fileName));

	_infiniteColorEngine = deviceConfig["infiniteColorEngine"].toBool(false);

	Debug(_log, "Infinite color engine resolution: {:s}", (_infiniteColorEngine) ? "true": "false");

	_enable_ice_rgbw = deviceConfig["enable_ice_rgbw"].toBool(false);
	_ice_white_mixer_threshold = deviceConfig["ice_white_mixer_threshold"].toDouble(0.0);
	_ice_white_led_intensity = deviceConfig["ice_white_led_intensity"].toDouble(1.8);
	_ice_white_temperatur.x = deviceConfig["ice_white_temperatur_red"].toDouble(1.0);
	_ice_white_temperatur.y = deviceConfig["ice_white_temperatur_green"].toDouble(1.0);
	_ice_white_temperatur.z = deviceConfig["ice_white_temperatur_blue"].toDouble(1.0);
	Debug(_log, "Infinite Color Engine RGBW is: {:s}, white channel temp for the white LED: {:s}, white mixer threshold: {:f}, white LED intensity: {:f}",
		((_enable_ice_rgbw) ? "enabled" : "disabled"), ColorSpaceMath::vecToString(_ice_white_temperatur), _ice_white_mixer_threshold, _ice_white_led_intensity);

	return initOK;
}

int DriverOtherFile::open()
{
	int retval = -1;
	_isDeviceReady = false;

	Debug(_log, "Open filename: {:s}", (_fileName));

	if (!_file->isOpen())
	{
		Debug(_log, "QIODevice::WriteOnly, {:s}", (_fileName));
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
			Debug(_log, "File: {:s}", (_fileName));
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
	if (nonlinearRgbColors->empty())
	{
		return { _infiniteColorEngine || _enable_ice_rgbw, 0 };
	}

	if (_infiniteColorEngine || _enable_ice_rgbw)
		return { true, writeColors(nullptr, nonlinearRgbColors) };
	else
		return { false,0 };
}

int DriverOtherFile::writeColors(const std::vector<ColorRgb>* ledValues, const SharedOutputColors& infinityLedColors)
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
	else if (infinityLedColors != nullptr)
	{
		QString separator = "";
		out.setRealNumberNotation(QTextStream::FixedNotation);
		out.setRealNumberPrecision(4);


		if (_enable_ice_rgbw) {
			_ledBuffer.resize(infinityLedColors->size() * 4);

			// RGBW by Infinite Color Engine
			_infiniteColorEngineRgbw.renderRgbwFrame(*infinityLedColors, _currentInterval, _ice_white_mixer_threshold, _ice_white_led_intensity, _ice_white_temperatur, _ledBuffer, 0, _colorOrder);

			auto start = _ledBuffer.data();
			auto end = start + _ledBuffer.size() - 4;
			auto format = [&](uint8_t v) { return QString("%1").arg(v);};

			for (uint8_t* current = start; current <= end; current += 4)
			{
				out << std::exchange(separator, ", ") <<  "{" << format(current[0]) << "," << format(current[1]) << "," << format(current[2]) << "," << format(current[3]) << "}";
			}
		}
		else {
			for (auto& color : *infinityLedColors)
			{
				auto format = [](float value) {return QString("%1").arg(value, 8, 'f', 4, ' '); };
				out << std::exchange(separator, ", ") << "{" << format(color.x * 255.f) << ", " << format(color.y * 255.f) << ", " << format(color.z * 255.f) << "}";
			}
		}
		result = infinityLedColors->size();
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
