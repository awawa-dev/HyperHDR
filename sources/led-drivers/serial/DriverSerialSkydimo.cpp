#include <led-drivers/serial/DriverSerialSkydimo.h>
#include <QtEndian>

#include <cassert>

DriverSerialSkydimo::DriverSerialSkydimo(const QJsonObject& deviceConfig)
	: ProviderSerial(deviceConfig)
	, _headerSize(6)
{
}

LedDevice* DriverSerialSkydimo::construct(const QJsonObject& deviceConfig)
{
	return new DriverSerialSkydimo(deviceConfig);
}

bool DriverSerialSkydimo::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSerial::init(deviceConfig))
	{
		CreateHeader();

		isInitOK = true;
	}
	return isInitOK;
}

void DriverSerialSkydimo::CreateHeader()
{
	auto finalSize = (uint64_t)_headerSize + _ledRGBCount;
	_ledBuffer.resize(finalSize, 0x00);

	_ledBuffer[0] = 'A';
	_ledBuffer[1] = 'd';
	_ledBuffer[2] = 'a';
	_ledBuffer[3] = 0;
	_ledBuffer[4] = 0;
	_ledBuffer[5] = std::min(_ledCount, 255u);

	Debug(_log, "Skydimo header for %d leds: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", _ledCount,
		_ledBuffer[0], _ledBuffer[1], _ledBuffer[2], _ledBuffer[3], _ledBuffer[4], _ledBuffer[5]);
}

int DriverSerialSkydimo::write(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Skydimo led count has changed (old: %d, new: %d). Rebuilding header.", _ledCount, ledValues.size());
		_ledCount = (uint)ledValues.size();
		_ledRGBCount = _ledCount * 3;
		CreateHeader();
	}
	
	auto bufferLength = _headerSize + ledValues.size() * sizeof(ColorRgb);

	if (bufferLength > _ledBuffer.size())
	{
		Warning(_log, "Skydimo buffer's size has changed. Skipping refresh.");
		return 0;
	}

	uint8_t* writer = _ledBuffer.data() + _headerSize;

	memcpy(writer, ledValues.data(), ledValues.size() * sizeof(ColorRgb));
	writer += ledValues.size() * sizeof(ColorRgb);
		
	bufferLength = writer - _ledBuffer.data();

	return writeBytes(bufferLength, _ledBuffer.data());	
}


bool DriverSerialSkydimo::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("skydimo", "leds_group_3_serial", DriverSerialSkydimo::construct);
