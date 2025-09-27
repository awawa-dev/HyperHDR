/* ProviderSpiGeneric.cpp
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
#include <led-drivers/spi/ProviderSpiGeneric.h>
#include <utils/Logger.h>

#include <QDirIterator>

ProviderSpiGeneric::ProviderSpiGeneric(Logger* logger)
	: QObject(), ProviderSpiInterface(logger)
{
}


ProviderSpiGeneric::~ProviderSpiGeneric()
{
	close();
}

bool ProviderSpiGeneric::init(QJsonObject deviceConfig)
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
	Debug(_log, "Real speed: %d", getRate());
	Debug(_log, "Inverted: %s, Mode: %d", (_spiDataInvert) ? "yes" : "no", _spiMode);

	return true;
}

QString ProviderSpiGeneric::open()
{
	QString error;

	const int bitsPerWord = 8;

	_fid = ::open(QSTRING_CSTR(_deviceName), O_RDWR);

	if (_fid < 0)
	{
		error = QString("Failed to open device (%1). Error message: %2").arg(_deviceName, strerror(errno));
	}
	else
	{
		if (ioctl(_fid, SPI_IOC_WR_MODE, &_spiMode) == -1 || ioctl(_fid, SPI_IOC_RD_MODE, &_spiMode) == -1)
		{
			error = "Cannot set SPI mode";
		}
		else
		{
			if (ioctl(_fid, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord) == -1 || ioctl(_fid, SPI_IOC_RD_BITS_PER_WORD, &bitsPerWord) == -1)
			{
				error = "Cannot set SPI bits per word";
			}
			else
			{
				if (ioctl(_fid, SPI_IOC_WR_MAX_SPEED_HZ, &_baudRate_Hz) == -1 || ioctl(_fid, SPI_IOC_RD_MAX_SPEED_HZ, &_baudRate_Hz) == -1)
				{
					error = "Cannot set SPI baudrate";
				}				
			}
		}
	}

	return error;
}

int ProviderSpiGeneric::close()
{
	Debug(_log, "Closing SPI interface");

	if (_fid > -1 && ::close(_fid) != 0)
	{
		Error(_log, "Failed to close device (%s). Error message: %s", QSTRING_CSTR(_deviceName), strerror(errno));
		return -1;
	}

	return 0;
}

int ProviderSpiGeneric::writeBytes(unsigned size, const uint8_t* data)
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

int ProviderSpiGeneric::getRate()
{
	return _baudRate_Hz;
}

QString ProviderSpiGeneric::getSpiType()
{
	return _spiType;
}

ProviderSpiInterface::SpiProvider ProviderSpiGeneric::getProviderType()
{
	return ProviderSpiInterface::SpiProvider::GENERIC;
}

QJsonObject ProviderSpiGeneric::discover(const QJsonObject& /*params*/)
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

	devicesDiscovered.insert("devices", deviceList);

	Debug(_log, "Generic SPI devices discovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}
