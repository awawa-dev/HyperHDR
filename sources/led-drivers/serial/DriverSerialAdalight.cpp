#include <led-drivers/serial/DriverSerialAdalight.h>
#include <QtEndian>

#include <cassert>

DriverSerialAdalight::DriverSerialAdalight(const QJsonObject& deviceConfig)
	: ProviderSerial(deviceConfig)
	, _headerSize(6)
	, _ligthBerryAPA102Mode(false)
	, _awa_mode(false)
{
	_white_channel_calibration = false;
	_white_channel_limit = 255;
	_white_channel_red = 255;
	_white_channel_green = 255;
	_white_channel_blue = 255;
}

LedDevice* DriverSerialAdalight::construct(const QJsonObject& deviceConfig)
{
	return new DriverSerialAdalight(deviceConfig);
}

bool DriverSerialAdalight::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSerial::init(deviceConfig))
	{

		_ligthBerryAPA102Mode = deviceConfig["lightberry_apa102_mode"].toBool(false);
		_awa_mode = deviceConfig["awa_mode"].toBool(false);

		_white_channel_calibration = deviceConfig["white_channel_calibration"].toBool(false);
		_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
		_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
		_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
		_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

		CreateHeader();

		if (_white_channel_calibration && _awa_mode)
			Debug(_log, "White channel limit: %i, red: %i, green: %i, blue: %i", _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);

		isInitOK = true;
	}
	return isInitOK;
}

void DriverSerialAdalight::CreateHeader()
{
	// create ledBuffer
	unsigned int totalLedCount = _ledCount;

	if (_ligthBerryAPA102Mode && !_awa_mode)
	{
		const unsigned int startFrameSize = 4;
		const unsigned int bytesPerRGBLed = 4;
		const unsigned int endFrameSize = qMax<unsigned int>(((_ledCount + 15) / 16), bytesPerRGBLed);
		_ledBuffer.resize((uint64_t)_headerSize + (((uint64_t)_ledCount) * bytesPerRGBLed) + startFrameSize + endFrameSize, 0x00);

		// init constant data values
		for (uint64_t iLed = 1; iLed <= static_cast<uint64_t>(_ledCount); iLed++)
		{
			_ledBuffer[iLed * 4 + _headerSize] = 0xFF;
		}
		Debug(_log, "Adalight driver with activated LightBerry APA102 mode");
	}
	else
	{
		_ligthBerryAPA102Mode = false;
		totalLedCount -= 1;
		auto finalSize = (uint64_t)_headerSize + _ledRGBCount + ((_awa_mode) ? 8 : 0);
		_ledBuffer.resize(finalSize, 0x00);

		if (_awa_mode)
			Debug(_log, "Adalight driver with activated high speeed & data integration check AWA protocol");
	}

	_ledBuffer[0] = 'A';
	_ledBuffer[1] = (_awa_mode) ? 'w' : 'd';
	_ledBuffer[2] = (_awa_mode && _white_channel_calibration) ? 'A' : 'a';
	qToBigEndian<quint16>(static_cast<quint16>(totalLedCount), &_ledBuffer[3]);
	_ledBuffer[5] = _ledBuffer[3] ^ _ledBuffer[4] ^ 0x55; // Checksum

	Debug(_log, "Adalight header for %d leds: %c%c%c 0x%02x 0x%02x 0x%02x", _ledCount,
		_ledBuffer[0], _ledBuffer[1], _ledBuffer[2], _ledBuffer[3], _ledBuffer[4], _ledBuffer[5]);
}

int DriverSerialAdalight::write(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Adalight led count has changed (old: %d, new: %d). Rebuilding header.", _ledCount, ledValues.size());
		_ledCount = (uint)ledValues.size();
		_ledRGBCount = _ledCount * 3;
		CreateHeader();
	}

	if (_ligthBerryAPA102Mode)
	{
		for (uint64_t iLed = 1; iLed <= static_cast<uint64_t>(_ledCount); iLed++)
		{
			const ColorRgb& rgb = ledValues[iLed - 1];
			_ledBuffer[iLed * 4 + 7] = rgb.red;
			_ledBuffer[iLed * 4 + 8] = rgb.green;
			_ledBuffer[iLed * 4 + 9] = rgb.blue;
		}

		return writeBytes(_ledBuffer.size(), _ledBuffer.data());
	}
	else
	{
		auto bufferLength = _headerSize + ledValues.size() * sizeof(ColorRgb) + ((_awa_mode) ? 8 : 0);

		if (bufferLength > _ledBuffer.size())
		{
			Warning(_log, "Adalight buffer's size has changed. Skipping refresh.");
			return 0;
		}

		uint8_t* writer = _ledBuffer.data() + _headerSize;
		uint8_t* hasher = writer;

		memcpy(writer, ledValues.data(), ledValues.size() * sizeof(ColorRgb));
		writer += ledValues.size() * sizeof(ColorRgb);

		if (_awa_mode)
		{
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
		}
		bufferLength = writer - _ledBuffer.data();

		return writeBytes(bufferLength, _ledBuffer.data());
	}
}

void DriverSerialAdalight::whiteChannelExtension(uint8_t*& writer)
{
	if (_awa_mode && _white_channel_calibration)
	{
		*(writer++) = _white_channel_limit;
		*(writer++) = _white_channel_red;
		*(writer++) = _white_channel_green;
		*(writer++) = _white_channel_blue;
	}
}

bool DriverSerialAdalight::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("adalight", "leds_group_3_serial", DriverSerialAdalight::construct);
