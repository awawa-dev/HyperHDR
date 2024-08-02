#include <led-drivers/serial/DriverSerialDMX.h>

#include <QSerialPort>
#ifndef _WIN32
	#include <time.h>
#endif

DriverSerialDMX::DriverSerialDMX(const QJsonObject& deviceConfig)
	: ProviderSerial(deviceConfig)
	, _dmxDeviceType(0)
	, _dmxStart(1)
	, _dmxSlotsPerLed(3)
	, _dmxLedCount(0)
	, _dmxChannelCount(0)
{
}

LedDevice* DriverSerialDMX::construct(const QJsonObject& deviceConfig)
{
	return new DriverSerialDMX(deviceConfig);
}

bool DriverSerialDMX::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSerial::init(deviceConfig))
	{
		QString dmxTypeString = deviceConfig["dmxtype"].toString("invalid");
		if (dmxTypeString == "raw")
		{
			_dmxDeviceType = 0;
			_dmxStart = 1;
			_dmxSlotsPerLed = 3;
		}
		else if (dmxTypeString == "McCrypt")
		{
			_dmxDeviceType = 1;
			_dmxStart = 1;
			_dmxSlotsPerLed = 4;
		}
		else
		{
			//Error(_log, "unknown dmx device type %s", QSTRING_CSTR(dmxString));
			QString errortext = QString("unknown dmx device type: %1").arg(dmxTypeString);
			this->setInError(errortext);
			return false;
		}

		Debug(_log, "_dmxTypeString \"%s\", _dmxDeviceType %d", QSTRING_CSTR(dmxTypeString), _dmxDeviceType);
		_serialPort->setStopBits(QSerialPort::TwoStop);

		_dmxLedCount = qMin(static_cast<int>(_ledCount), 512 / _dmxSlotsPerLed);
		_dmxChannelCount = 1 + _dmxSlotsPerLed * _dmxLedCount;

		Debug(_log, "_dmxStart %d, _dmxSlotsPerLed %d", _dmxStart, _dmxSlotsPerLed);
		Debug(_log, "_ledCount %d, _dmxLedCount %d, _dmxChannelCount %d", _ledCount, _dmxLedCount, _dmxChannelCount);

		_ledBuffer.resize(_dmxChannelCount, 0);
		_ledBuffer[0] = 0x00;	// NULL START code

		isInitOK = true;
	}
	return isInitOK;
}

int DriverSerialDMX::write(const std::vector<ColorRgb>& ledValues)
{
	switch (_dmxDeviceType) {
	case 0:
		memcpy(_ledBuffer.data() + 1, ledValues.data(), _dmxChannelCount - 1);
		break;
	case 1:
		int l = _dmxStart;
		for (int d = 0; d < _dmxLedCount; d++)
		{
			_ledBuffer[l++] = ledValues[d].red;
			_ledBuffer[l++] = ledValues[d].green;
			_ledBuffer[l++] = ledValues[d].blue;
			_ledBuffer[l++] = 255;
		}
		break;
	}

	_serialPort->setBreakEnabled(true);
	// Note Windows: There is no concept of ns sleeptime, the closest possible is 1ms but requested is 0,000176ms
#ifndef _WIN32
	nanosleep((const struct timespec[]) { {0, 176000L} }, NULL);	// 176 uSec break time
#endif
	_serialPort->setBreakEnabled(false);
#ifndef _WIN32
	nanosleep((const struct timespec[]) { {0, 12000L} }, NULL);	// 176 uSec make after break time
#endif

#undef uberdebug
#ifdef uberdebug
	printf("Writing %d bytes", _dmxChannelCount);
	for (unsigned int i = 0; i < _dmxChannelCount; i++)
	{
		if (i % 32 == 0) {
			printf("\n%04x: ", i);
		}
		printf("%02x ", _ledBuffer[i]);
	}
	printf("\n");
#endif

	return writeBytes(_dmxChannelCount, _ledBuffer.data());
}

bool DriverSerialDMX::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("dmx", "leds_group_3_serial", DriverSerialDMX::construct);
