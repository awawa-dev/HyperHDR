/* ProviderSpiInterface.h
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

#pragma once

#include <QJsonObject>
#include <led-drivers/LedDevice.h>

class ProviderSpiInterface
{
public:
	enum SpiProvider { NONE, GENERIC, FTDI, LIB_FTDI };

	ProviderSpiInterface(LoggerName logger) :
		_deviceName("/dev/spidev0.0")
		, _baudRate_Hz(1000000)
		, _fid(-1)
		, _spiDataInvert(false)
		, _spiMode(0)
		, _spiType("")
		, _log(logger)
	{
	};

	virtual bool init(QJsonObject deviceConfig) = 0;
	virtual ~ProviderSpiInterface() {};

	virtual QString open() = 0;
	virtual int close() = 0;

	virtual QJsonObject discover(const QJsonObject& params) = 0;

	virtual int writeBytes(unsigned size, const uint8_t* data) = 0;

	virtual int getRate() = 0;
	virtual QString getSpiType() = 0;
	virtual SpiProvider getProviderType() = 0;

protected:
	QString _deviceName;
	int _baudRate_Hz;
	int _fid;
	bool _spiDataInvert;
	int _spiMode;

	QString _spiType;
	LoggerName _log;
};
