/* ProviderSpiFtdi.cpp
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

#include <cstring>
#include <led-drivers/spi/ProviderSpiFtdi.h>
#include <utils/Logger.h>

#include <tchar.h>
#include <libloaderapi.h>
#include <wchar.h>

namespace
{
	constexpr auto* FTDI_DLL = _T("ftd2xx.dll");
}

ProviderSpiFtdi::ProviderSpiFtdi(const LoggerName& logger)
	: QObject(), ProviderSpiInterface(logger),
		_dllHandle(nullptr),
		_deviceHandle(0),

		_fun_FT_ListDevices(nullptr),
		_fun_FT_OpenEx(nullptr),
		_fun_FT_Write(nullptr),
		_fun_FT_ResetDevice(nullptr),
		_fun_FT_SetChars(nullptr),
		_fun_FT_SetBitMode(nullptr),
		_fun_FT_Close(nullptr),
		_fun_FT_SetBaudRate(nullptr),
		_fun_FT_SetUSBParameters(nullptr),
		_fun_FT_SetLatencyTimer(nullptr),
		_fun_FT_SetFlowControl(nullptr)
{
}

#define LOAD_PROC(FtdiProc) \
if (!error && ((_fun_##FtdiProc = reinterpret_cast<PTR_##FtdiProc>(GetProcAddress(_dllHandle, #FtdiProc))) == NULL)) \
{ \
	error = true; \
	Error(_log, "Unable to load " #FtdiProc "procedure"); \
}

bool ProviderSpiFtdi::loadLibrary()
{
	if (_dllHandle != nullptr)
	{
		return true;
	}

	_dllHandle = LoadLibrary(FTDI_DLL);

	if (!_dllHandle || _dllHandle == INVALID_HANDLE_VALUE)
	{
		_dllHandle = nullptr;
		Error(_log, "Unable to load ftd2xx.dll library");
	}
	else
	{
		bool error = false;

		LOAD_PROC(FT_ListDevices);
		LOAD_PROC(FT_OpenEx);
		LOAD_PROC(FT_Write);
		LOAD_PROC(FT_ResetDevice);
		LOAD_PROC(FT_SetChars);
		LOAD_PROC(FT_SetBitMode);
		LOAD_PROC(FT_Close);
		LOAD_PROC(FT_SetBaudRate);
		LOAD_PROC(FT_SetUSBParameters);
		LOAD_PROC(FT_SetLatencyTimer);
		LOAD_PROC(FT_SetFlowControl);
		
		if (error)
		{
			FreeLibrary(_dllHandle);
			_fun_FT_ListDevices = nullptr;
			_dllHandle = nullptr;
		}
	}

	return _dllHandle != nullptr;
}

ProviderSpiFtdi::~ProviderSpiFtdi()
{
	close();

	if (_dllHandle != nullptr)
	{
		FreeLibrary(_dllHandle);
		_dllHandle = nullptr;
	}
}

bool ProviderSpiFtdi::init(QJsonObject deviceConfig)
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

	Debug(_log, "Speed: {:d}, Type: {:s}", _baudRate_Hz, (_spiType));
	Debug(_log, "Real speed: {:d}", getRate());
	Debug(_log, "Inverted: {:s}, Mode: {:d}", (_spiDataInvert) ? "yes" : "no", _spiMode);	

	isInitOK = loadLibrary();
	
	return isInitOK;
}

QString ProviderSpiFtdi::open()
{
	QString error;

	bool isInt = false;
	long long deviceLocation = _deviceName.toLong(&isInt, 10);

	if (!isInt)
	{
		return "The device name is not a FTDI path (must be a number)";
	}

	if (_fun_FT_OpenEx(reinterpret_cast<PVOID>(deviceLocation), FT_OPEN_BY_LOCATION, &_deviceHandle) != FT_OK)
	{
		_deviceHandle = 0;
		return "Cannot open selected FTDI device";
	}

	Debug(_log, "Initializing MPSSE interface...");	

	if (error.isEmpty() && _fun_FT_ResetDevice(_deviceHandle) != FT_OK)
	{
		error = "ResetDevice did not return properly";
	}

	if (error.isEmpty() && _fun_FT_SetBaudRate(_deviceHandle, 4000000) != FT_OK)
	{
		error = "FT_SetBaudRate did not return properly";
	}

	if (error.isEmpty() && _fun_FT_SetUSBParameters(_deviceHandle, 65536, 65535) != FT_OK)
	{
		error = "FT_SetUSBParameters failed";
	}

	if (error.isEmpty() && _fun_FT_SetChars(_deviceHandle, false, 0, false, 0) != FT_OK)
	{
		error = "FT_SetChars failed";
	}

	if (error.isEmpty() && _fun_FT_SetLatencyTimer(_deviceHandle, 1) != FT_OK)
	{
		error = "FT_SetLatencyTimer failed";
	}

	if (error.isEmpty() && _fun_FT_SetFlowControl(_deviceHandle, FT_FLOW_NONE, 0x00, 0x00) != FT_OK)
	{
		error = "FT_SetFlowControl failed";
	}

	if (error.isEmpty() && _fun_FT_SetBitMode(_deviceHandle, 0x0, 0x00) != FT_OK)
	{
		error = "FT_SetBitMode failed";
	}

	if (error.isEmpty() && _fun_FT_SetBitMode(_deviceHandle, 0xff, 0x02) != FT_OK)
	{
		error = "FT_SetBitMode MPSSE failed";
	}

	if (error.isEmpty())
	{
		DWORD dwNumBytesSent = 0;
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

		if (_fun_FT_Write(_deviceHandle, command.data(), static_cast<DWORD>(command.size()), &dwNumBytesSent) != FT_OK)
		{
			error = "Cannot initilize SPI interface";
		}
	}

	if (!error.isEmpty())
	{
		_fun_FT_Close(_deviceHandle);
		_deviceHandle = 0;
	}

	return error;
}

int ProviderSpiFtdi::close()
{
	if (_deviceHandle)
	{
		_fun_FT_Close(_deviceHandle);
		_deviceHandle = 0;
	}
	
	return 0;
}

int ProviderSpiFtdi::writeBytes(unsigned size, const uint8_t* data)
{
	DWORD dwNumBytesSent = 0;
	std::vector<uint8_t> command;

	// cs & clock low
	command.push_back(0x80);
	command.push_back(0);
	command.push_back(0x08 | 0x02 | 0x01);
	_fun_FT_Write(_deviceHandle, command.data(), static_cast<DWORD>(command.size()), &dwNumBytesSent);

	command.push_back(0x11);
	command.push_back((size - 1) & 0xFF);
	command.push_back(((size - 1) >> 8) & 0xFF);
	_fun_FT_Write(_deviceHandle, command.data(), static_cast<DWORD>(command.size()), &dwNumBytesSent);
	if (_fun_FT_Write(_deviceHandle, const_cast<uint8_t*>(data), size, &dwNumBytesSent) != FT_OK)
	{
		Error(_log, "The FTDI device reports error while writing");
		return -1;
	}

	// cs high
	command.clear();
	command.push_back(0x80);
	command.push_back(0x08);
	command.push_back(0x08 | 0x02 | 0x01);
	_fun_FT_Write(_deviceHandle, command.data(), static_cast<DWORD>(command.size()), &dwNumBytesSent);


	return dwNumBytesSent;
}

int ProviderSpiFtdi::getRate()
{
	long divisor = int(std::ceil((30000000.0 - float(_baudRate_Hz)) / float(_baudRate_Hz))) & 0xFFFF;
	return std::ceil(30000000.0 / (1 + divisor));
}

QString ProviderSpiFtdi::getSpiType()
{
	return _spiType;
}

ProviderSpiInterface::SpiProvider ProviderSpiFtdi::getProviderType()
{
	return ProviderSpiInterface::SpiProvider::FTDI;
}

QJsonObject ProviderSpiFtdi::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;

	if (loadLibrary())
	{
		long deviceIds[16]{};

		DWORD numDevs = 0;
		auto ftStatus = _fun_FT_ListDevices(deviceIds, &numDevs, FT_LIST_ALL | FT_OPEN_BY_LOCATION);

		if (ftStatus == FT_OK)
		{
			QJsonArray deviceList;
			QStringList files;

			for (DWORD i = 0, count = std::min(numDevs, DWORD(std::size(deviceIds))); i < count; ++i)
			{
				deviceList.push_back(QJsonObject{
					{"value", QJsonValue(static_cast<qint64>(deviceIds[i]))},
					{"name", QString("FTDI SPI device location: %1").arg(QString::number(deviceIds[i]))}
					});
			}

			devicesDiscovered.insert("devices", deviceList);

			Debug(_log, "FTDI SPI devices discovered: [{:s}]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());			
		}
		else
		{
			Debug(_log, "No FTDI  SPI devices found");
		}
	}

	return devicesDiscovered;
}
