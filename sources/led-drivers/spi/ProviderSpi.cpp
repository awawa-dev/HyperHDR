/* ProviderSpi.cpp
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

#include <HyperhdrConfig.h>
#include <algorithm>
#include <led-drivers/spi/ProviderSpi.h>
#include <utils/Logger.h>


#ifdef ENABLE_SPIFTDI
	#ifdef WIN32
		#include <led-drivers/spi/ProviderSpiFtdi.h>
	#else
		#include <led-drivers/spi/ProviderSpiLibFtdi.h>
	#endif
#endif

#if !defined(WIN32)
	#include <unistd.h>
#endif

#if !defined(WIN32) && !defined(APPLE)
	#include <led-drivers/spi/ProviderSpiGeneric.h>
#endif

ProviderSpi::ProviderSpi(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
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
		bool isInt = false;
		#ifdef ENABLE_SPIFTDI			
			deviceConfig["output"].toString().toLong(&isInt, 10);
			if (isInt)
			{
				#ifdef WIN32
					_provider = std::make_unique<ProviderSpiFtdi>(_log);
				#else
					_provider = std::make_unique<ProviderSpiLibFtdi>(_log);
				#endif
			}
		#endif

		#if !defined(WIN32) && !defined(APPLE)
			if (isInt)
			{
				_provider = std::make_unique<ProviderSpiGeneric>(_log);
			}
		#endif

		if (_defaultInterval > 0)
		{
			Warning(_log, "The refresh timer is enabled ('Refresh time' > 0) and may limit the performance of the LED driver. Ignore this error if you set it on purpose for some reason (but you almost never need it).");
		}

		isInitOK = (_provider != nullptr) && _provider->init(deviceConfig);
	}
	return isInitOK;
}

int ProviderSpi::open()
{
	_isDeviceReady = false;	

	if (_provider == nullptr)
	{
		this->setInError("open: no SPI provider");
		return -1;
	}

	QString errortext = _provider->open();


	if (!errortext.isEmpty())
	{
		this->setInError(errortext);
		return -1;
	}
	else
	{
		_isDeviceReady = true;
	}

	return 0;
}

int ProviderSpi::close()
{	
	_isDeviceReady = false;

	if (_provider == nullptr)
	{
		Error(_log, "close: no SPI provider");
		return -1;
	}

	_provider->close();
	
	return 0;
}

int ProviderSpi::writeBytes(unsigned size, const uint8_t* data)
{	
	if (_provider == nullptr)
	{
		Error(_log, "writeBytes: no SPI provider");
		return -1;
	}
	else
	{
		return _provider->writeBytes(size, data);
	}
}

int ProviderSpi::writeBytesEsp8266(unsigned size, const uint8_t* data)
{
	const int32_t BUFFER_SIZE = 34;

	uint8_t* startData = (uint8_t*)data;
	uint8_t* endData = (uint8_t*)data + size;
	uint8_t buffer[BUFFER_SIZE];

	int retVal = 0;

	while (retVal >= 0 && startData < endData)
	{
		memset(buffer, 0, sizeof(buffer));
		buffer[0] = 2;
		buffer[1] = 0;
		for (int i = 0; i < BUFFER_SIZE - 2 && startData < endData; i++, startData++)
		{
			buffer[2 + i] = *startData;
		}
		retVal = writeBytes(sizeof(buffer), buffer);
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
	
	int retVal = 0;

	while (retVal >= 0 && startData < endData)
	{
		#if !defined(WIN32)
			if (startData != data)
			{
				usleep(1000);
			}
		#endif

		int sent = std::min(REAL_BUFFER, static_cast<int>(endData - startData));
		memset(buffer, 0, sizeof(buffer));
		memcpy(buffer, startData, sent);
		startData += sent;
		buffer[REAL_BUFFER] = 0xAA;
		retVal = writeBytes(sizeof(buffer), buffer);
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

	int retVal = 0;

	while (retVal >= 0 && startData < endData)
	{
		#if !defined(WIN32)
			if (startData != data)
			{
				usleep(1000);
			}
		#endif

		int sent = std::min(REAL_BUFFER, static_cast<int>(endData - startData));
		memset(buffer, 0, sizeof(buffer));
		memcpy(buffer, startData, sent);
		startData += sent;
		retVal = writeBytes(sizeof(buffer), buffer);
		ErrorIf((retVal < 0), _log, "SPI failed to write. errno: %d, %s", errno, strerror(errno));
	}

	return retVal;
}

int ProviderSpi::getRate() {
	if (_provider == nullptr)
	{
		Error(_log, "getRate: no SPI provider");
		return -1;
	}
	else
	{
		return _provider->getRate();
	}
}

QString ProviderSpi::getSpiType() {
	if (_provider == nullptr)
	{
		Error(_log, "getSpiType: no SPI provider");
		return "";
	}
	else
	{
		return _provider->getSpiType();
	}
}

QJsonObject ProviderSpi::discover(const QJsonObject& /*params*/)
{
	QJsonObject params;
	std::list<std::unique_ptr<ProviderSpiInterface>> discoveryList;


	#if !defined(WIN32) && !defined(APPLE)
		discoveryList.push_back(std::make_unique<ProviderSpiGeneric>(_log));
	#endif

	#ifdef ENABLE_SPIFTDI
		#ifdef WIN32
			discoveryList.push_back(std::make_unique<ProviderSpiFtdi>(_log));
		#else
			discoveryList.push_back(std::make_unique<ProviderSpiLibFtdi>(_log));
		#endif
	#endif

	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	QStringList files;

	for(auto & item : discoveryList)
	{
		auto ret = item->discover(params);
		if (ret.contains("devices") && ret["devices"].isArray())
		{
			auto arr = ret["devices"].toArray();
			for (const auto& val : arr)
			{
				deviceList.push_back(val);
			}
		}
	}

	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	devicesDiscovered.insert("devices", deviceList);

	Debug(_log, "SPI devices discovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());	

	return devicesDiscovered;
}
