#include <led-drivers/spi/DriverSpiSk6812SPI.h>

DriverSpiSk6812SPI::DriverSpiSk6812SPI(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
	, _whiteAlgorithm(RGBW::WhiteAlgorithm::HYPERSERIAL_COLD_WHITE)
	, _white_channel_limit(255)
	, _white_channel_red(255)
	, _white_channel_green(255)
	, _white_channel_blue(255)
	, SPI_BYTES_PER_COLOUR(4)
	, bitpair_to_byte{
		0b10001000,
		0b10001100,
		0b11001000,
		0b11001100}
{
}

LedDevice* DriverSpiSk6812SPI::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiSk6812SPI(deviceConfig);
}

bool DriverSpiSk6812SPI::init(const QJsonObject& deviceConfig)
{
	deviceConfig["rate"] = 3000000;

	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
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

			auto rateHz = getRate();

			WarningIf((rateHz < 2050000 || rateHz > 4000000), _log, "SPI rate %d outside recommended range (2050000 -> 4000000)", rateHz);

			const int SPI_FRAME_END_LATCH_BYTES = 3;
			_ledBuffer.resize(_ledRGBWCount * SPI_BYTES_PER_COLOUR + SPI_FRAME_END_LATCH_BYTES, 0x00);

			isInitOK = true;
		}
	}
	return isInitOK;
}

int DriverSpiSk6812SPI::write(const std::vector<ColorRgb>& ledValues)
{
	unsigned spi_ptr = 0;
	const int SPI_BYTES_PER_LED = sizeof(ColorRgbw) * SPI_BYTES_PER_COLOUR;

	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Sk6812SPI led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = ledValues.size();

		const int SPI_FRAME_END_LATCH_BYTES = 3;
		_ledBuffer.resize(0, 0x00);
		_ledBuffer.resize(_ledRGBWCount * SPI_BYTES_PER_COLOUR + SPI_FRAME_END_LATCH_BYTES, 0x00);
	}

	for (const ColorRgb& color : ledValues)
	{
		ColorRgbw tempRgbw;

		RGBW::rgb2rgbw(color, &tempRgbw, _whiteAlgorithm, channelCorrection);
		uint32_t colorBits =
			((uint32_t)tempRgbw.red << 24) +
			((uint32_t)tempRgbw.green << 16) +
			((uint32_t)tempRgbw.blue << 8) +
			tempRgbw.white;

		for (int j = SPI_BYTES_PER_LED - 1; j >= 0; j--)
		{
			_ledBuffer[spi_ptr + j] = bitpair_to_byte[colorBits & 0x3];
			colorBits >>= 2;
		}
		spi_ptr += SPI_BYTES_PER_LED;
	}

	_ledBuffer[spi_ptr++] = 0;
	_ledBuffer[spi_ptr++] = 0;
	_ledBuffer[spi_ptr++] = 0;

	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

bool DriverSpiSk6812SPI::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("sk6812spi", "leds_group_0_SPI", DriverSpiSk6812SPI::construct);
