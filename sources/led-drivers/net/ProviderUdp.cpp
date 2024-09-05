/* ProviderUdp.cpp
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

// STL includes
#include <cstring>
#include <cstdio>
#include <iostream>
#include <exception>
// Linux includes
#include <fcntl.h>

#include <QStringList>
#include <QUdpSocket>
#include <QHostInfo>

// Local HyperHDR includes
#include <led-drivers/net/ProviderUdp.h>

ProviderUdp::ProviderUdp(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _udpSocket(nullptr)
	, _port(1)
	, _defaultHost("127.0.0.1")
{
}

ProviderUdp::~ProviderUdp() = default;

bool ProviderUdp::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (LedDevice::init(deviceConfig))
	{
		QString host = deviceConfig["host"].toString(_defaultHost);

		if (_address.setAddress(host))
		{
			Debug(_log, "Successfully parsed %s as an IP-address.", QSTRING_CSTR(_address.toString()));
		}
		else
		{
			QHostInfo hostInfo = QHostInfo::fromName(host);
			if (hostInfo.error() == QHostInfo::NoError)
			{
				_address = hostInfo.addresses().first();
				Debug(_log, "Successfully resolved IP-address (%s) for hostname (%s).", QSTRING_CSTR(_address.toString()), QSTRING_CSTR(host));
			}
			else
			{
				QString errortext = QString("Failed resolving IP-address for [%1], (%2) %3").arg(host).arg(hostInfo.error()).arg(hostInfo.errorString());
				this->setInError(errortext);
				isInitOK = false;
			}
		}

		if (!_isDeviceInError)
		{
			int config_port = deviceConfig["port"].toInt(_port);
			if (config_port <= 0 || config_port > 0xffff)
			{
				QString errortext = QString("Invalid target port [%1]!").arg(config_port);
				this->setInError(errortext);
				isInitOK = false;
			}
			else
			{
				_port = static_cast<quint16>(config_port);
				Debug(_log, "UDP socket will write to %s:%u", QSTRING_CSTR(_address.toString()), _port);

				_udpSocket = std::unique_ptr<QUdpSocket>(new QUdpSocket());

				isInitOK = true;
			}
		}
	}
	return isInitOK;
}

int ProviderUdp::open()
{
	int retval = -1;
	_isDeviceReady = false;

	// Try to bind the UDP-Socket
	if (_udpSocket != nullptr)
	{
		if (_udpSocket->state() != QAbstractSocket::BoundState)
		{
			QHostAddress localAddress = QHostAddress::Any;
			quint16      localPort = 0;
			if (!_udpSocket->bind(localAddress, localPort))
			{
				QString warntext = QString("Could not bind local address: %1, (%2) %3").arg(localAddress.toString()).arg(_udpSocket->error()).arg(_udpSocket->errorString());
				Warning(_log, "%s", QSTRING_CSTR(warntext));
			}
		}
		// Everything is OK, device is ready
		_isDeviceReady = true;
		retval = 0;
	}
	else
	{
		Warning(_log, "Open error. The UDP socket instance is not initialised yet. Ignoring.");
	}
	return retval;
}

int ProviderUdp::close()
{
	int retval = 0;
	_isDeviceReady = false;

	if (_udpSocket != nullptr)
	{
		// Test, if device requires closing
		if (_udpSocket->isOpen())
		{
			Debug(_log, "Close UDP-device: %s", QSTRING_CSTR(this->getActiveDeviceType()));
			_udpSocket->close();
			// Everything is OK -> device is closed
		}
	}
	return retval;
}

int ProviderUdp::writeBytes(const unsigned size, const uint8_t* data)
{
	int rc = 0;
	qint64 bytesWritten = _udpSocket->writeDatagram(reinterpret_cast<const char*>(data), size, _address, _port);

	if (bytesWritten == -1 || bytesWritten != size)
	{
		Warning(_log, "%s", QSTRING_CSTR(QString("(%1:%2) Write Error: (%3) %4").arg(_address.toString()).arg(_port).arg(_udpSocket->error()).arg(_udpSocket->errorString())));
		rc = -1;
	}
	return  rc;
}

int ProviderUdp::writeBytes(const QByteArray& bytes)
{
	int rc = 0;
	qint64 bytesWritten = _udpSocket->writeDatagram(bytes, _address, _port);

	if (bytesWritten == -1 || bytesWritten != bytes.size())
	{
		Warning(_log, "%s", QSTRING_CSTR(QString("(%1:%2) Write Error: (%3) %4").arg(_address.toString()).arg(_port).arg(_udpSocket->error()).arg(_udpSocket->errorString())));
		rc = -1;
	}
	return  rc;
}

void ProviderUdp::setPort(int port)
{
	if (port > 0 && port <= 0xffff && _port != port)
	{
		_port = port;
		Debug(_log, "Updated port to: %i", _port);
	}
}
