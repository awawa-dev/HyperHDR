#include "LedDeviceAdalight.h"

#include <QtEndian>

#include <cassert>

LedDeviceAdalight::LedDeviceAdalight(const QJsonObject &deviceConfig)
	: ProviderRs232(deviceConfig)
	  , _headerSize(6)
	  , _ligthBerryAPA102Mode(false)
{
}

LedDevice* LedDeviceAdalight::construct(const QJsonObject &deviceConfig)
{
	return new LedDeviceAdalight(deviceConfig);
}

bool LedDeviceAdalight::init(const QJsonObject &deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if ( ProviderRs232::init(deviceConfig) )
	{

		_ligthBerryAPA102Mode = deviceConfig["lightberry_apa102_mode"].toBool(false);
		_awa_mode = deviceConfig["awa_mode"].toBool(false);
		// create ledBuffer
		unsigned int totalLedCount = _ledCount;

		if (_ligthBerryAPA102Mode && !_awa_mode)
		{
			const unsigned int startFrameSize = 4;
			const unsigned int bytesPerRGBLed = 4;
			const unsigned int endFrameSize = qMax<unsigned int>(((_ledCount + 15) / 16), bytesPerRGBLed);
			_ledBuffer.resize(_headerSize + (_ledCount * bytesPerRGBLed) + startFrameSize + endFrameSize, 0x00);

			// init constant data values
			for (signed iLed=1; iLed<= static_cast<int>( _ledCount); iLed++)
			{
				_ledBuffer[iLed*4+_headerSize] = 0xFF;
			}
			Debug( _log, "Adalight driver with activated LightBerry APA102 mode");
		}
		else
		{
			_ligthBerryAPA102Mode = false;
			totalLedCount -= 1;
			_ledBuffer.resize((uint16_t)_headerSize + (uint16_t)_ledRGBCount + (uint16_t)((_awa_mode)?2:0), 0x00);

			if (_awa_mode)
				Debug(_log, "Adalight driver with activated high speeed & data integration check AWA protocol");			
		}

		_ledBuffer[0] = 'A';
		_ledBuffer[1] = (_awa_mode) ? 'w' : 'd';
		_ledBuffer[2] = 'a';
		qToBigEndian<quint16>(static_cast<quint16>(totalLedCount), &_ledBuffer[3]);
		_ledBuffer[5] = _ledBuffer[3] ^ _ledBuffer[4] ^ 0x55; // Checksum

		Debug( _log, "Adalight header for %d leds: %c%c%c 0x%02x 0x%02x 0x%02x", _ledCount,
			   _ledBuffer[0], _ledBuffer[1], _ledBuffer[2], _ledBuffer[3], _ledBuffer[4], _ledBuffer[5] );

		isInitOK = true;
	}
	return isInitOK;
}

int LedDeviceAdalight::write(const std::vector<ColorRgb> & ledValues)
{
	if(_ligthBerryAPA102Mode)
	{
		for (signed iLed=1; iLed<=static_cast<int>( _ledCount); iLed++)
		{
			const ColorRgb& rgb = ledValues[iLed-1];
			_ledBuffer[iLed*4+7] = rgb.red;
			_ledBuffer[iLed*4+8] = rgb.green;
			_ledBuffer[iLed*4+9] = rgb.blue;
		}
	}
	else
	{
		assert((_headerSize + ledValues.size() * sizeof(ColorRgb) + ((_awa_mode) ? 2 : 0)) <= _ledBuffer.size());

		
		memcpy(_headerSize + _ledBuffer.data(), ledValues.data(), ledValues.size() * sizeof(ColorRgb));

		if (_awa_mode)
		{			
			uint16_t fletcher1 = 0, fletcher2 = 0;
			uint8_t* input = (uint8_t*)ledValues.data();
			for (uint16_t iLed = 0; iLed < ledValues.size() * sizeof(ColorRgb); iLed++, input++)
			{
				fletcher1 = (fletcher1 + *input) % 255;
				fletcher2 = (fletcher2 + fletcher1) % 255;
			}
			_ledBuffer[_headerSize + ledValues.size() * sizeof(ColorRgb)  ] = fletcher1;
			_ledBuffer[_headerSize + ledValues.size() * sizeof(ColorRgb)+1] = fletcher2;
		}
	}

	int rc = writeBytes(_ledBuffer.size(), _ledBuffer.data());

	return rc;
}
