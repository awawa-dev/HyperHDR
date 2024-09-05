/* DriverSpiHyperSPI.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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

#include <led-drivers/spi/DriverSpiHyperSPI.h>
#include <QtEndian>

DriverSpiHyperSPI::DriverSpiHyperSPI(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
{
	_headerSize = 6;

	_white_channel_calibration = false;
	_white_channel_limit = 255;
	_white_channel_red = 255;
	_white_channel_green = 255;
	_white_channel_blue = 255;
}

LedDevice* DriverSpiHyperSPI::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiHyperSPI(deviceConfig);
}

bool DriverSpiHyperSPI::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		_white_channel_calibration = deviceConfig["white_channel_calibration"].toBool(false);
		_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
		_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
		_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
		_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

		createHeader();

		if (_white_channel_calibration)
			Debug(_log, "White channel limit: %i, red: %i, green: %i, blue: %i", _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);

		isInitOK = true;
	}

	return isInitOK;
}

void DriverSpiHyperSPI::createHeader()
{
	unsigned int totalLedCount = _ledCount - 1;

	auto finalSize = (uint64_t)_headerSize + (_ledCount * 3) + 8;
	_ledBuffer.resize(finalSize, 0x00);

	Debug(_log, "SPI driver with data integration check AWA protocol");

	_ledBuffer[0] = 'A';
	_ledBuffer[1] = 'w';
	_ledBuffer[2] = (_white_channel_calibration) ? 'A' : 'a';
	qToBigEndian<quint16>(static_cast<quint16>(totalLedCount), &_ledBuffer[3]);
	_ledBuffer[5] = _ledBuffer[3] ^ _ledBuffer[4] ^ 0x55; // Checksum

	Debug(_log, "SPI header for %d leds: %c%c%c 0x%02x 0x%02x 0x%02x", _ledCount,
		_ledBuffer[0], _ledBuffer[1], _ledBuffer[2], _ledBuffer[3], _ledBuffer[4], _ledBuffer[5]);
}

int DriverSpiHyperSPI::write(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "AWA spi led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());

		_ledCount = ledValues.size();

		createHeader();
	}

	auto bufferLength = _headerSize + ledValues.size() * sizeof(ColorRgb) + 8;

	if (bufferLength > _ledBuffer.size())
	{
		Warning(_log, "AWA SPI buffer's size has changed. Skipping refresh.");

		return 0;
	}

	uint8_t* writer = _ledBuffer.data() + _headerSize;
	uint8_t* hasher = writer;

	memcpy(writer, ledValues.data(), ledValues.size() * sizeof(ColorRgb));
	writer += ledValues.size() * sizeof(ColorRgb);

	whiteChannelExtension(writer);

	uint16_t fletcher1 = 0, fletcher2 = 0, fletcherExt = 0;
	uint8_t position = 0;
	while (hasher < writer)
	{
		fletcherExt = (fletcherExt + (*(hasher) ^ (position++))) % 255;
		fletcher1 = (fletcher1 + *(hasher++)) % 255;
		fletcher2 = (fletcher2 + fletcher1) % 255;
	}
	*(writer++) = (uint8_t)fletcher1;
	*(writer++) = (uint8_t)fletcher2;
	*(writer++) = (uint8_t)((fletcherExt != 0x41) ? fletcherExt : 0xaa);
	bufferLength = writer -_ledBuffer.data();

	if (_spiType == "esp8266")
		return writeBytesEsp8266(bufferLength, _ledBuffer.data());
	else if (_spiType == "esp32")
		return writeBytesEsp32(bufferLength, _ledBuffer.data());
	else if (_spiType == "rp2040")
		return writeBytesRp2040(bufferLength, _ledBuffer.data());
	else
		return writeBytes(bufferLength, _ledBuffer.data());
}

void DriverSpiHyperSPI::whiteChannelExtension(uint8_t*& writer)
{
	if (_white_channel_calibration)
	{
		*(writer++) = _white_channel_limit;
		*(writer++) = _white_channel_red;
		*(writer++) = _white_channel_green;
		*(writer++) = _white_channel_blue;
	}
}

bool DriverSpiHyperSPI::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("hyperspi", "leds_group_0_SPI", DriverSpiHyperSPI::construct);//awa_spi
