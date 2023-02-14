
// STL includes
#include <cstring>
#include <cstdio>
#include <iostream>
#include <cerrno>

// Linux includes
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

// Local HyperHDR includes
#include "ProviderSpi.h"
#include <utils/Logger.h>

#include <QDirIterator>

ProviderSpi::ProviderSpi(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _deviceName("/dev/spidev0.0")
	, _baudRate_Hz(1000000)
	, _fid(-1)
	, _spiMode(SPI_MODE_0)
	, _spiDataInvert(false)
	, _spiType("")
{
	memset(&_spi, 0, sizeof(_spi));
}

ProviderSpi::~ProviderSpi()
{
}

bool ProviderSpi::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (LedDevice::init(deviceConfig))
	{
		_deviceName = deviceConfig["output"].toString(_deviceName);
		_spiType = deviceConfig["spitype"].toString("");
		_baudRate_Hz = deviceConfig["rate"].toInt(_baudRate_Hz);
		_spiMode = deviceConfig["spimode"].toInt(_spiMode);
		_spiDataInvert = deviceConfig["invert"].toBool(_spiDataInvert);

		Debug(_log, "_baudRate_Hz [%d], _spiType: %s", _baudRate_Hz, QSTRING_CSTR(_spiType));
		Debug(_log, "_spiDataInvert [%d], _spiMode [%d]", _spiDataInvert, _spiMode);

		isInitOK = true;
	}
	return isInitOK;
}

int ProviderSpi::open()
{
	int retval = -1;
	QString errortext;
	_isDeviceReady = false;

	const int bitsPerWord = 8;

	_fid = ::open(QSTRING_CSTR(_deviceName), O_RDWR);

	if (_fid < 0)
	{
		errortext = QString("Failed to open device (%1). Error message: %2").arg(_deviceName, strerror(errno));
		retval = -1;
	}
	else
	{
		if (ioctl(_fid, SPI_IOC_WR_MODE, &_spiMode) == -1 || ioctl(_fid, SPI_IOC_RD_MODE, &_spiMode) == -1)
		{
			retval = -2;
		}
		else
		{
			if (ioctl(_fid, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord) == -1 || ioctl(_fid, SPI_IOC_RD_BITS_PER_WORD, &bitsPerWord) == -1)
			{
				retval = -4;
			}
			else
			{
				if (ioctl(_fid, SPI_IOC_WR_MAX_SPEED_HZ, &_baudRate_Hz) == -1 || ioctl(_fid, SPI_IOC_RD_MAX_SPEED_HZ, &_baudRate_Hz) == -1)
				{
					retval = -6;
				}
				else
				{
					// Everything OK -> enable device
					_isDeviceReady = true;
					retval = 0;
				}
			}
		}
		if (retval < 0)
		{
			errortext = QString("Failed to open device (%1). Error Code: %2").arg(_deviceName).arg(retval);
		}
	}

	if (retval < 0)
	{
		this->setInError(errortext);
	}

	return retval;
}

int ProviderSpi::close()
{
	// LedDevice specific closing activities
	int retval = 0;
	_isDeviceReady = false;

	// Test, if device requires closing
	if (_fid > -1)
	{
		// Close device
		if (::close(_fid) != 0)
		{
			Error(_log, "Failed to close device (%s). Error message: %s", QSTRING_CSTR(_deviceName), strerror(errno));
			retval = -1;
		}
	}
	return retval;
}

int ProviderSpi::writeBytes(unsigned size, const uint8_t* data)
{
	uint8_t* newdata = nullptr;

	if (_fid < 0)
	{
		return -1;
	}

	_spi.tx_buf = __u64(data);
	_spi.len = __u32(size);

	if (_spiDataInvert)
	{
		newdata = (uint8_t*)malloc(size);
		for (unsigned i = 0; i < size; i++) {
			newdata[i] = data[i] ^ 0xff;
		}
		_spi.tx_buf = __u64(newdata);
	}

	int retVal = ioctl(_fid, SPI_IOC_MESSAGE(1), &_spi);
	ErrorIf((retVal < 0), _log, "SPI failed to write. errno: %d, %s", errno, strerror(errno));

	if (newdata != nullptr)
		free(newdata);

	return retVal;
}

int ProviderSpi::writeBytesEsp8266(unsigned size, const uint8_t* data)
{
	uint8_t* startData = (uint8_t*)data;
	uint8_t* endData = (uint8_t*)data + size;
	uint8_t buffer[34];

	if (_fid < 0)
	{
		return -1;
	}

	_spi.tx_buf = __u64(&buffer);
	_spi.len = __u32(34);
	_spi.delay_usecs = 0;

	int retVal = 0;

	while (retVal >= 0 && startData < endData)
	{
		memset(buffer, 0, sizeof(buffer));
		buffer[0] = 2;
		buffer[1] = 0;
		for (int i = 0; i < 32 && startData < endData; i++, startData++)
		{
			buffer[2 + i] = *startData;
		}
		retVal = ioctl(_fid, SPI_IOC_MESSAGE(1), &_spi);
		ErrorIf((retVal < 0), _log, "SPI failed to write. errno: %d, %s", errno, strerror(errno));
	}

	return retVal;
}

int ProviderSpi::writeBytesEsp32(unsigned size, const uint8_t* data)
{
	static const int      REAL_BUFFER = 1536;
	static const uint32_t BUFFER_SIZE = REAL_BUFFER + 8;

	uint8_t* startData = (uint8_t*)data;
	uint8_t* endData = (uint8_t*)data + size;
	uint8_t buffer[BUFFER_SIZE];

	if (_fid < 0)
	{
		return -1;
	}

	_spi.tx_buf = __u64(&buffer);
	_spi.len = __u32(BUFFER_SIZE);
	_spi.delay_usecs = 0;

	int retVal = 0;

	while (retVal >= 0 && startData < endData)
	{
		memset(buffer, 0, sizeof(buffer));
		for (int i = 0; i < REAL_BUFFER && startData < endData; i++, startData++)
		{
			buffer[i] = *startData;
		}
		buffer[REAL_BUFFER] = 0xAA;
		retVal = ioctl(_fid, SPI_IOC_MESSAGE(1), &_spi);
		ErrorIf((retVal < 0), _log, "SPI failed to write. errno: %d, %s", errno, strerror(errno));
	}

	return retVal;
}

QJsonObject ProviderSpi::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	QStringList files;	
	QDirIterator it("/dev", QStringList() << "spidev*", QDir::System);
	
	while (it.hasNext())
		files << it.next();
	files.sort();

	for (const auto& path : files)
		deviceList.push_back(QJsonObject{
			{"value", path},
			{ "name", path } });

	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	devicesDiscovered.insert("devices", deviceList);

	Debug(_log, "SPI devices discovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}
