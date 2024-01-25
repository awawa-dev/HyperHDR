#pragma once

/* EspTools.h
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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

class EspTools
{
public:

	static void goingSleep(QSerialPort* _serialPort)
	{
		uint8_t comBuffer[] = { 0x41, 0x77, 0x41, 0x2a, 0xa2, 0x35, 0x68, 0x79, 0x70, 0x65, 0x72, 0x68, 0x64, 0x72 };
		_serialPort->write((char*)comBuffer, sizeof(comBuffer));
	}

	static void initializeEsp(QSerialPort* _serialPort, QSerialPortInfo& serialPortInfo, Logger*& _log, bool _forceSerialDetection)
	{
		uint8_t comBuffer[] = { 0x41, 0x77, 0x41, 0x2a, 0xa2, 0x15, 0x68, 0x79, 0x70, 0x65, 0x72, 0x68, 0x64, 0x72 };

		if (serialPortInfo.productIdentifier() == 0xa && serialPortInfo.vendorIdentifier() == 0x2e8a)
		{
			Warning(_log, "Detected Rp2040 type board. HyperHDR skips the reset. State: %i, %i",
				_serialPort->isDataTerminalReady(), _serialPort->isRequestToSend());

			_serialPort->write((char*)comBuffer, sizeof(comBuffer));

			_serialPort->setDataTerminalReady(true);
			_serialPort->setRequestToSend(true);
			_serialPort->setRequestToSend(false);
		}
		else if (serialPortInfo.productIdentifier() == 0x80c2 && serialPortInfo.vendorIdentifier() == 0x303a)
		{
			Warning(_log, "Detected ESP32-S2 lolin mini type board. HyperHDR skips the reset. State: %i, %i",
				_serialPort->isDataTerminalReady(), _serialPort->isRequestToSend());

			_serialPort->write((char*)comBuffer, sizeof(comBuffer));

			_serialPort->setDataTerminalReady(true);
			_serialPort->setRequestToSend(true);
			_serialPort->setRequestToSend(false);
		}
		else if (serialPortInfo.productIdentifier() == 0x3483 && serialPortInfo.vendorIdentifier() == 0x1106)
		{
			Warning(_log, "Enabling the Rpi4 udev bug workaround. The serial device is incorrectly identified by the OS and HyperHDR skips the reset. State: %i, %i",
				_serialPort->isDataTerminalReady(), _serialPort->isRequestToSend());

			_serialPort->write((char*)comBuffer, sizeof(comBuffer));

			_serialPort->setDataTerminalReady(true);
			_serialPort->setRequestToSend(true);
			_serialPort->setRequestToSend(false);
		}
		else if (_forceSerialDetection)
		{
			Warning(_log, "Force ESP/Pico detection override enabled. HyperHDR skips the reset. State: %i, %i",
				_serialPort->isDataTerminalReady(), _serialPort->isRequestToSend());

			_serialPort->write((char*)comBuffer, sizeof(comBuffer));

			_serialPort->setDataTerminalReady(true);
			_serialPort->setRequestToSend(true);
			_serialPort->setRequestToSend(false);
		}
		else
		{
			// reset to defaults
			_serialPort->setDataTerminalReady(true);
			_serialPort->setRequestToSend(false);
			QThread::msleep(50);

			// reset device
			_serialPort->setDataTerminalReady(false);
			_serialPort->setRequestToSend(true);
			QThread::msleep(150);

			// resume device
			_serialPort->setRequestToSend(false);
			QThread::msleep(100);
		}

		// read the reset message, search for AWA tag
		auto start = InternalClock::now();

		while (InternalClock::now() - start < 1000)
		{
			_serialPort->waitForReadyRead(100);
			if (_serialPort->bytesAvailable() > 16)
			{
				auto incoming = _serialPort->readAll();
				for (int i = 0; i < incoming.length(); i++)
					if (!(incoming[i] == '\n' ||
						(incoming[i] >= ' ' && incoming[i] <= 'Z') ||
						(incoming[i] >= 'a' && incoming[i] <= 'z')))
					{
						incoming.replace(incoming[i], '*');
					}
				QString result = QString(incoming).remove('*').replace('\n', ' ').trimmed();
				if (result.indexOf("Awa driver", Qt::CaseInsensitive) >= 0)
				{
					Info(_log, "DETECTED DEVICE USING HyperSerialEsp8266/HyperSerialESP32/HyperSerialPico FIRMWARE (%s) at %i msec", QSTRING_CSTR(result), int(InternalClock::now() - start));
					start = 0;
					break;
				}
				else
					Info(_log, "ESP sent: '%s'", QSTRING_CSTR(result));
			}
		}

		if (start != 0)
			Error(_log, "Could not detect HyperSerialEsp8266/HyperSerialESP32/HyperSerialPico device");
	}
};

