/* DriverRpiPio.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#include <led-drivers/pwm/rpi_pio/DriverRpiPio.h>
#include <linalg.h>

DriverRpiPio::DriverRpiPio(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _isRgbw(false)
	, _whiteAlgorithm(RGBW::WhiteAlgorithm::HYPERSERIAL_COLD_WHITE)
	, _white_channel_limit(255)
	, _white_channel_red(255)
	, _white_channel_green(255)
	, _white_channel_blue(255)
{
}

bool DriverRpiPio::init(QJsonObject deviceConfig)
{
	QString errortext;

	bool isInitOK = false;

	// Initialise sub-class
	if (LedDevice::init(deviceConfig))
	{
		_output = deviceConfig["output"].toString("/dev/null");
		_isRgbw = deviceConfig["rgbw"].toBool(false);

		Debug(_log, "Rp1/PIO LED module path : %s", QSTRING_CSTR(_output));
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
				errortext = QString("unknown whiteAlgorithm: %1").arg(whiteAlgorithm);
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

int DriverRpiPio::open()
{
	int retval = -1;
	_isDeviceReady = false;

	QFileInfo fi(_output);
    if (!fi.exists())
	{
		Error(_log, "The device does not exists: %s", QSTRING_CSTR(_output));
		Error(_log, "Must be configured first like for ex: dtoverlay=ws2812-pio,gpio=18,num_leds=30,rgbw in /boot/firmware/config.txt. rgbw only for sk6812 RGBW. num_leds is your LED number. Only RPI5+");
		return retval;
	}	
	
	if (!fi.isWritable())
	{
		Error(_log, "The device is not writable. Are you root or your user has write access rights to: %s", QSTRING_CSTR(_output));
		return retval;
	}

	QFile renderer(_output);
	if (!renderer.open(QIODevice::WriteOnly))
	{
		Error(_log, "Cannot open the device for writing: %s", QSTRING_CSTR(_output));
		return retval;
	}	
	else
	{
		renderer.close();
		_isDeviceReady = true;
		retval = 0;
	}
	return retval;
}

int DriverRpiPio::close()
{
	int retval = 0;
	_isDeviceReady = false;

	return retval;
}

int DriverRpiPio::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	QByteArray render;

	QFile renderer(_output);
	if (!renderer.open(QIODevice::WriteOnly))
	{
		this->setInError("Cannot open the device for writing");
		return 0;
	}

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

	auto written = renderer.write(render);
	renderer.close();

	return written;
}

LedDevice* DriverRpiPio::construct(const QJsonObject& deviceConfig)
{
	return new DriverRpiPio(deviceConfig);
}

bool DriverRpiPio::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("rpi_pio", "leds_group_1_PWM", DriverRpiPio::construct);
