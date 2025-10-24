#include <led-drivers/pwm/rpi_pio/DriverRpi2040.h>
#include <linalg.h>

DriverRpi2040::DriverRpi2040(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _isRgbw(false)
	, _whiteAlgorithm(RGBW::WhiteAlgorithm::HYPERSERIAL_COLD_WHITE)
	, _white_channel_limit(255)
	, _white_channel_red(255)
	, _white_channel_green(255)
	, _white_channel_blue(255)
{
}

bool DriverRpi2040::init(QJsonObject deviceConfig)
{
	QString errortext;

	bool isInitOK = false;

	// Initialise sub-class
	if (LedDevice::init(deviceConfig))
	{
		_output = deviceConfig["output"].toString("/dev/null");
		_isRgbw = deviceConfig["rgbw"].toBool(false);

		Debug(_log, "Rp1/rp2040 LED module path : %s", QSTRING_CSTR(_output));
		Debug(_log, "Type : %s", (_isRgbw) ? "RGBW" : "RGB");

		if (_isRgbw)
		{
			QString whiteAlgorithm = deviceConfig["white_algorithm"].toString("hyperserial_cold_white");

			_whiteAlgorithm = RGBW::stringToWhiteAlgorithm(whiteAlgorithm);
			_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
			_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
			_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
			_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

			if (_whiteAlgorithm == RGBW::WhiteAlgorithm::INVALID)
			{
				QString errortext = QString("unknown whiteAlgorithm: %1").arg(whiteAlgorithm);
				this->setInError(errortext);
				isInitOK = false;
			}
			else
			{
				Debug(_log, "white_algorithm : %s", QSTRING_CSTR(whiteAlgorithm));

				if (_whiteAlgorithm == RGBW::WhiteAlgorithm::HYPERSERIAL_CUSTOM)
				{
					Debug(_log, "White channel limit: %i, red: %i, green: %i, blue: %i", _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);
				}

				if (_whiteAlgorithm == RGBW::WhiteAlgorithm::HYPERSERIAL_CUSTOM ||
					_whiteAlgorithm == RGBW::WhiteAlgorithm::HYPERSERIAL_NEUTRAL_WHITE ||
					_whiteAlgorithm == RGBW::WhiteAlgorithm::HYPERSERIAL_COLD_WHITE)
				{
					RGBW::prepareRgbwCalibration(channelCorrection, _whiteAlgorithm, _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);
				}

				isInitOK = true;
			}
		}
		else
		{
			isInitOK = true;
		}
	}

	if (!isInitOK)
	{
		this->setInError(errortext);
	}
	return isInitOK;
}

int DriverRpi2040::open()
{
	int retval = -1;
	_isDeviceReady = false;

	QFileInfo fi(_output);
    if (!fi.exists())
	{
		Error(_log, "The device does not exists: %s", QSTRING_CSTR(_output));
		return retval;
	}	
	
	if (!fi.isWritable())
	{
		Error(_log, "The device is not writable");
		return retval;
	}

	_renderer = std::make_unique<QFile>(_output);
	if (!_renderer->open(QIODevice::WriteOnly))
	{
		_renderer = nullptr;
		Error(_log, "Cannot open the device for writing: %s", QSTRING_CSTR(_output));
		return retval;
	}	
	else
	{
		_isDeviceReady = true;
		retval = 0;
	}
	return retval;
}

int DriverRpi2040::close()
{
	int retval = 0;
	_isDeviceReady = false;

	if (_renderer)
	{
		_renderer->close();
		_renderer = nullptr;
	}

	return retval;
}

int DriverRpi2040::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	QByteArray render;

	if (_isRgbw)
	{
		render.reserve(ledValues.size() * 4);
		for (const ColorRgb& color : ledValues)
		{
			ColorRgbw tempRgbw{};

			tempRgbw.red = color.red;
			tempRgbw.green = color.green;
			tempRgbw.blue = color.blue;
			tempRgbw.white = 0;

			rgb2rgbw(color, &tempRgbw, _whiteAlgorithm, channelCorrection);

			render.append(static_cast<char>(tempRgbw.red));
			render.append(static_cast<char>(tempRgbw.green));
			render.append(static_cast<char>(tempRgbw.blue));
			render.append(static_cast<char>(tempRgbw.white));
		}
	}
	else
	{
		render.reserve(ledValues.size() * 3);
		for (const ColorRgb& color : ledValues)
		{
			render.append(static_cast<char>(color.red));
			render.append(static_cast<char>(color.green));
			render.append(static_cast<char>(color.blue));
		}
	}

	auto written = _renderer->write(render);
	_renderer->flush();

	return written;
}

LedDevice* DriverRpi2040::construct(const QJsonObject& deviceConfig)
{
	return new DriverRpi2040(deviceConfig);
}

bool DriverRpi2040::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("rp2040", "leds_group_1_PWM", DriverRpi2040::construct);
