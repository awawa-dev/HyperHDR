
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
#include <led-drivers/spi/ProviderSpi.h>
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

		if (_spiType == "rp2040" && _baudRate_Hz > 20833333)
		{
			_baudRate_Hz = 20833333;
		}

		Debug(_log, "Speed: %d, Type: %s", _baudRate_Hz, QSTRING_CSTR(_spiType));
		Debug(_log, "Inverted: %s, Mode: %d", (_spiDataInvert) ? "yes" : "no", _spiMode);

		if (_defaultInterval > 0)
			Warning(_log, "The refresh timer is enabled ('Refresh time' > 0) and may limit the performance of the LED driver. Ignore this error if you set it on purpose for some reason (but you almost never need it).");

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
					uint8_t rpBuffer[] = { 0x41, 0x77, 0x41, 0x2a, 0xa2, 0x15, 0x68, 0x79, 0x70, 0x65, 0x72, 0x68, 0x64, 0x72 };

					if (_spiType == "rp2040")
					{
						writeBytesRp2040(sizeof(rpBuffer), rpBuffer);
					}
					else if (_spiType == "esp32")
					{
						writeBytesEsp32(sizeof(rpBuffer), rpBuffer);
					}

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
	uint8_t rpBuffer[] = { 0x41, 0x77, 0x41, 0x2a, 0xa2, 0x35, 0x68, 0x79, 0x70, 0x65, 0x72, 0x68, 0x64, 0x72 };
	int retval = 0;

	_isDeviceReady = false;

	Debug(_log, "Closing SPI interface");

	if (_spiType == "rp2040")
	{
		writeBytesRp2040(sizeof(rpBuffer), rpBuffer);
	}

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
	MemoryBuffer<uint8_t> buffer;
	spi_ioc_transfer _spi;

	memset(&_spi, 0, sizeof(_spi));

	if (_fid < 0)
	{
		return -1;
	}

	_spi.tx_buf = __u64(data);
	_spi.len = __u32(size);

	if (_spiDataInvert)
	{
		buffer.resize(size);
		for (unsigned i = 0; i < size; i++) {
			buffer.data()[i] = data[i] ^ 0xff;
		}
		_spi.tx_buf = __u64(buffer.data());
	}

	int retVal = ioctl(_fid, SPI_IOC_MESSAGE(1), &_spi);
	ErrorIf((retVal < 0), _log, "SPI failed to write. errno: %d, %s", errno, strerror(errno));

	return retVal;
}

int ProviderSpi::writeBytesEsp8266(unsigned size, const uint8_t* data)
{
	uint8_t* startData = (uint8_t*)data;
	uint8_t* endData = (uint8_t*)data + size;
	uint8_t buffer[34];
	spi_ioc_transfer _spi;

	memset(&_spi, 0, sizeof(_spi));

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
	const int      REAL_BUFFER = 1536;
	const uint32_t BUFFER_SIZE = REAL_BUFFER + 8;

	uint8_t* startData = (uint8_t*)data;
	uint8_t* endData = (uint8_t*)data + size;
	uint8_t buffer[BUFFER_SIZE];
	spi_ioc_transfer _spi;

	memset(&_spi, 0, sizeof(_spi));

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
		if (startData != data)
			usleep(1000);

		int sent = std::min(REAL_BUFFER, static_cast<int>(endData - startData));
		memset(buffer, 0, sizeof(buffer));
		memcpy(buffer, startData, sent);
		startData += sent;
		buffer[REAL_BUFFER] = 0xAA;
		retVal = ioctl(_fid, SPI_IOC_MESSAGE(1), &_spi);
		ErrorIf((retVal < 0), _log, "SPI failed to write. errno: %d, %s", errno, strerror(errno));
	}

	return retVal;
}

int ProviderSpi::writeBytesRp2040(unsigned size, const uint8_t* data)
{
	static const int      REAL_BUFFER = 1536;
	static const uint32_t BUFFER_SIZE = REAL_BUFFER;

	uint8_t* startData = (uint8_t*)data;
	uint8_t* endData = (uint8_t*)data + size;
	uint8_t buffer[BUFFER_SIZE];
	spi_ioc_transfer _spi;

	memset(&_spi, 0, sizeof(_spi));

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
		if (startData != data)
			usleep(1000);

		int sent = std::min(REAL_BUFFER, static_cast<int>(endData - startData));
		memset(buffer, 0, sizeof(buffer));
		memcpy(buffer, startData, sent);
		startData += sent;
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
