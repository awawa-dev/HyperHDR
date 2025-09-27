/* ProviderSpiLibFtdi.cpp
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

#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <cassert>
#include <sys/wait.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <cstring>
#include <iostream>
#include <dlfcn.h>
#include <utility>

#include <led-drivers/spi/ProviderSpiLibFtdi.h>
#include <utils/Logger.h>

namespace
{
	constexpr auto* LIBFTDI_CANON = "libftdi1.so";
	constexpr auto* LIBFTDI_ALT = "libftdi1.so.2";
}

ProviderSpiLibFtdi::ProviderSpiLibFtdi(Logger* logger)
	: QObject(), ProviderSpiInterface(logger),
	_dllHandle(nullptr),
	_deviceHandle(nullptr),
	_fun_ftdi_new(nullptr),
	_fun_ftdi_usb_open_bus_addr(nullptr),
	_fun_ftdi_free(nullptr),
	_fun_ftdi_usb_reset(nullptr),
	_fun_ftdi_set_baudrate(nullptr),
	_fun_ftdi_write_data_set_chunksize(nullptr),
	_fun_ftdi_set_event_char(nullptr),
	_fun_ftdi_set_error_char(nullptr),
	_fun_ftdi_set_latency_timer(nullptr),
	_fun_ftdi_setflowctrl(nullptr),
	_fun_ftdi_set_bitmode(nullptr),
	_fun_ftdi_write_data(nullptr),
	_fun_ftdi_usb_close(nullptr),
	_fun_ftdi_usb_find_all(nullptr),
	_fun_ftdi_list_free(nullptr),
	_fun_ftdi_get_error_string(nullptr)
{
}

#define LOAD_PROC(FtdiProc) \
if (!error && ((_fun_##FtdiProc = reinterpret_cast<PTR_##FtdiProc>(dlsym(_dllHandle, #FtdiProc))) == NULL)) \
{ \
	error = true; \
	Error(_log, "Unable to load " #FtdiProc "procedure"); \
}

bool ProviderSpiLibFtdi::loadLibrary()
{
	if (_dllHandle != nullptr)
	{
		return true;
	}

	if ((_dllHandle = dlopen(LIBFTDI_CANON, RTLD_NOW | RTLD_GLOBAL)) == nullptr)
	{
		_dllHandle = dlopen(LIBFTDI_ALT, RTLD_NOW | RTLD_GLOBAL);
	}

	if (_dllHandle == nullptr)
	{
		Error(_log, "Unable to load %s nor %s library", LIBFTDI_CANON, LIBFTDI_ALT);
	}
	else
	{
		bool error = false;

		LOAD_PROC(ftdi_new);
		LOAD_PROC(ftdi_usb_open_bus_addr);
		LOAD_PROC(ftdi_free);
		LOAD_PROC(ftdi_usb_reset);
		LOAD_PROC(ftdi_set_baudrate);
		LOAD_PROC(ftdi_write_data_set_chunksize);
		LOAD_PROC(ftdi_set_event_char);
		LOAD_PROC(ftdi_set_error_char);
		LOAD_PROC(ftdi_set_latency_timer);
		LOAD_PROC(ftdi_setflowctrl);
		LOAD_PROC(ftdi_set_bitmode);
		LOAD_PROC(ftdi_write_data);
		LOAD_PROC(ftdi_usb_close);
		LOAD_PROC(ftdi_usb_find_all);
		LOAD_PROC(ftdi_list_free);
		LOAD_PROC(ftdi_get_error_string);

		if (error)
		{
			dlclose(_dllHandle);
			_dllHandle = nullptr;
		}
	}

	return _dllHandle != nullptr;
}

ProviderSpiLibFtdi::~ProviderSpiLibFtdi()
{
	close();

	if (_dllHandle != nullptr)
	{
		dlclose(_dllHandle);
		_dllHandle = nullptr;
	}
}

bool ProviderSpiLibFtdi::init(QJsonObject deviceConfig)
{
	bool isInitOK = false;

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

	isInitOK = loadLibrary();

	return isInitOK;
}

QString ProviderSpiLibFtdi::open()
{
	QString error;

	bool isInt = false;
	long long deviceLocation = _deviceName.toLong(&isInt, 10);

	if (!isInt)
	{
		return "The device name is not a FTDI path (must be a number)";
	}

	if ((_deviceHandle = _fun_ftdi_new()) == nullptr)
	{
		return "libFTDI ftdi_new has failed";
	}

	if (_fun_ftdi_usb_open_bus_addr(_deviceHandle, (deviceLocation >> 8) & 0xff, (deviceLocation) & 0xff) < 0)
	{
		Error(_log, "libFTDI ftdi_usb_open_bus_addr has failed: %s", _fun_ftdi_get_error_string(_deviceHandle));

		_fun_ftdi_free(_deviceHandle);
		_deviceHandle = nullptr;

		return "libFTDI ftdi_usb_open_bus_addr has failed";
	}

	Debug(_log, "Initializing MPSSE interface...");

	if (error.isEmpty() && _fun_ftdi_usb_reset(_deviceHandle) < 0)
	{
		error = "libFTDI ftdi_usb_reset did not return properly";
	}

	if (error.isEmpty() && _fun_ftdi_set_baudrate(_deviceHandle, 4000000) < 0)
	{
		error = "libFTDI ftdi_set_baudrate did not return properly";
	}

	if (error.isEmpty() && _fun_ftdi_write_data_set_chunksize(_deviceHandle, 65535) < 0)
	{
		error = "libFTDI ftdi_usb_reset did not return properly";
	}

	if (error.isEmpty() && _fun_ftdi_set_event_char(_deviceHandle, 0, false) < 0)
	{
		error = "libFTDI ftdi_set_event_char did not return properly";
	}

	if (error.isEmpty() && _fun_ftdi_set_error_char(_deviceHandle, 0, false) < 0)
	{
		error = "libFTDI ftdi_set_error_char did not return properly";
	}


	if (error.isEmpty() && _fun_ftdi_set_latency_timer(_deviceHandle, 1) < 0)
	{
		error = "libFTDI ftdi_set_latency_timer did not return properly";
	}

	if (error.isEmpty() && _fun_ftdi_setflowctrl(_deviceHandle, 0x00) < 0)
	{
		error = "libFTDI ftdi_setflowctrl did not return properly";
	}

	if (error.isEmpty() && _fun_ftdi_set_bitmode(_deviceHandle, 0x0, 0x00) < 0)
	{
		error = "libFTDI ftdi_set_bitmode(0) did not return properly";
	}

	if (error.isEmpty() && _fun_ftdi_set_bitmode(_deviceHandle, 0xff, 0x02) < 0)
	{
		error = "libFTDI ftdi_set_bitmode(2) did not return properly";
	}

	if (error.isEmpty())
	{
		std::vector<uint8_t> command;

		command.push_back(0x8A);
		command.push_back(0x97);
		command.push_back(0x8D);

		long divisor = int(std::ceil((30000000.0 - float(_baudRate_Hz)) / float(_baudRate_Hz))) & 0xFFFF;
		command.push_back(0x86);
		command.push_back(divisor & 0xFF);
		command.push_back((divisor >> 8) & 0xFF);

		command.push_back(0x80);
		command.push_back(0x08);
		command.push_back(0x08 | 0x02 | 0x01);

		if (_fun_ftdi_write_data(_deviceHandle, command.data(), command.size()) < 0)
		{
			error = "Cannot initilize SPI interface";
		}
	}

	if (!error.isEmpty())
	{
		_fun_ftdi_usb_close(_deviceHandle);
		_fun_ftdi_free(_deviceHandle);
		_deviceHandle = nullptr;
	}

	return error;
}

int ProviderSpiLibFtdi::close()
{
	if (_deviceHandle)
	{
		_fun_ftdi_usb_close(_deviceHandle);
		_fun_ftdi_free(_deviceHandle);
		_deviceHandle = nullptr;
	}

	return 0;
}

int ProviderSpiLibFtdi::writeBytes(unsigned size, const uint8_t* data)
{
	std::vector<uint8_t> command;

	// cs & clock low
	command.push_back(0x80);
	command.push_back(0);
	command.push_back(0x08 | 0x02 | 0x01);
	_fun_ftdi_write_data(_deviceHandle, command.data(), command.size());

	command.push_back(0x11);
	command.push_back((size - 1) & 0xFF);
	command.push_back(((size - 1) >> 8) & 0xFF);
	_fun_ftdi_write_data(_deviceHandle, command.data(), command.size());
	if (_fun_ftdi_write_data(_deviceHandle, const_cast<uint8_t*>(data), size) < static_cast<int>(size))
	{
		Error(_log, "The FTDI device reports error while writing");
		return -1;
	}

	// cs high
	command.clear();
	command.push_back(0x80);
	command.push_back(0x08);
	command.push_back(0x08 | 0x02 | 0x01);
	_fun_ftdi_write_data(_deviceHandle, command.data(), command.size());


	return size;
}

int ProviderSpiLibFtdi::getRate()
{
	long divisor = int(std::ceil((30000000.0 - float(_baudRate_Hz)) / float(_baudRate_Hz))) & 0xFFFF;
	return std::ceil(30000000.0 / (1 + divisor));
}

QString ProviderSpiLibFtdi::getSpiType()
{
	return _spiType;
}

ProviderSpiInterface::SpiProvider ProviderSpiLibFtdi::getProviderType()
{
	return ProviderSpiInterface::SpiProvider::FTDI;
}

QJsonObject ProviderSpiLibFtdi::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;

	if (loadLibrary())
	{
		struct ftdi_device_list* devlist = nullptr;
		struct ftdi_context* ftdic = _fun_ftdi_new();

		if (ftdic == nullptr)
		{
			return devicesDiscovered;
		}

		int numDevs = _fun_ftdi_usb_find_all(ftdic, &devlist, 0, 0);

		if (numDevs < 0)
		{
			Debug(_log, "libFTDI ftdi_usb_find_all did not return properly");
		}
		else
		{
			if (numDevs > 0)
			{
				QJsonArray deviceList;
				QStringList files;

				struct ftdi_device_list* curDev = devlist;
				while (curDev)
				{
					long deviceLocation = ((curDev->dev->bus_number & 0xff) << 8) | (curDev->dev->device_address & 0xff);
					deviceList.push_back(QJsonObject{
						{"value", QJsonValue((qint64)deviceLocation)},
						{ "name", QString("libFTDI SPI device location: %1").arg(QString::number(deviceLocation)) } });
					curDev = curDev->next;
				}

				devicesDiscovered.insert("devices", deviceList);

				Debug(_log, "libFTDI SPI devices discovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());
			}
			else
			{
				Debug(_log, "No libFTDI SPI devices found");
			}

			_fun_ftdi_list_free(&devlist);
		}

		_fun_ftdi_free(ftdic);
	}

	return devicesDiscovered;
}
