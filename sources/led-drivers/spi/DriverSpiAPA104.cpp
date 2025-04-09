#include <led-drivers/spi/DriverSpiAPA104.h>

/*
From the data sheet:

(TH+TL=1.25μs±600ns)

T0H,	 0 code, high level time,	  350ns ±150ns
T0L,	 0 code, low level time,	 1360ns ±150ns
T1H,	 1 code, high level time,	 1360ns ±150ns
T1L,	 1 code, low level time,	  350ns ±150ns
WT,	 Wait for the processing time,	 NA
Trst,	 Reset code,low level time,	 24µs

To normalise the pulse times so they fit in 4 SPI bits:

On the assumption that the "low" time doesnt matter much

A SPI bit time of 0.40uS = 2.5 Mbit/sec
T0 is sent as 1000
T1 is sent as 1110

With a bit of excel testing, we can work out the maximum and minimum speeds:
2000000 MIN
2235000 AVG
2470000 MAX

Wait time:
Not Applicable for WS2812

Reset time:
using the max of 2470000, the bit time is 405nS
Reset time is 24uS = 59 bits = 8 bytes

*/

DriverSpiAPA104::DriverSpiAPA104(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
	, SPI_BYTES_PER_COLOUR(4)
	, SPI_FRAME_END_LATCH_BYTES(8)
	, bitpair_to_byte{
		0b10001000,
		0b10001110,
		0b11101000,
		0b11101110,
}
{
}


LedDevice* DriverSpiAPA104::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiAPA104(deviceConfig);
}

bool DriverSpiAPA104::init(const QJsonObject& deviceConfig)
{
	_baudRate_Hz = 2235000;

	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		WarningIf((_baudRate_Hz < 2000000 || _baudRate_Hz > 2470000), _log, "SPI rate %d outside recommended range (2000000 -> 2470000)", _baudRate_Hz);

		_ledBuffer.resize(_ledRGBCount * SPI_BYTES_PER_COLOUR + SPI_FRAME_END_LATCH_BYTES, 0x00);

		isInitOK = true;
	}
	return isInitOK;
}

int DriverSpiAPA104::write(const std::vector<ColorRgb>& ledValues)
{
	unsigned spi_ptr = 0;
	const int SPI_BYTES_PER_LED = sizeof(ColorRgb) * SPI_BYTES_PER_COLOUR;

	if (_ledCount != ledValues.size())
	{
		Warning(_log, "APA104 led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = ledValues.size();

		_ledBuffer.resize(0, 0x00);
		_ledBuffer.resize(_ledRGBCount * SPI_BYTES_PER_COLOUR + SPI_FRAME_END_LATCH_BYTES, 0x00);
	}

	for (const ColorRgb& color : ledValues)
	{
		uint32_t colorBits = ((unsigned int)color.red << 16)
			| ((unsigned int)color.green << 8)
			| color.blue;

		for (int j = SPI_BYTES_PER_LED - 1; j >= 0; j--)
		{
			_ledBuffer[spi_ptr + j] = bitpair_to_byte[colorBits & 0x3];
			colorBits >>= 2;
		}
		spi_ptr += SPI_BYTES_PER_LED;
	}

	for (int j = 0; j < SPI_FRAME_END_LATCH_BYTES; j++)
	{
		_ledBuffer[spi_ptr++] = 0;
	}

	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

bool DriverSpiAPA104::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("apa104", "leds_group_0_SPI", DriverSpiAPA104::construct);
