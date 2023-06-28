/* RawUdpServer.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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

// util
#include <utils/NetOrigin.h>
#include <utils/GlobalSignals.h>
#include <utils/RawUdpServer.h>
#include <base/HyperHdrInstance.h>

// qt
#include <QJsonObject>
#include <QUdpSocket>
#include <QCoreApplication>
#include <QNetworkDatagram>
#include <QTimer>

RawUdpServer::RawUdpServer(HyperHdrInstance* hyperhdr, const QJsonDocument& config, QObject* parent)
	: QObject(parent)
	, _server(new QUdpSocket(this))
	, _log(Logger::getInstance("RAW_UDP_SERVER"))
	, _port(0)
	, _priority(0)
	, _initialized(false)
	, _hyperhdr(hyperhdr)
	, _config(config)
	, _inactiveTimer(new QTimer(this))
{
	initServer();
}

RawUdpServer::~RawUdpServer()
{
	stopServer();

	delete _server;
}

void RawUdpServer::initServer()
{
	if (_server != nullptr)
	{
		connect(_server, &QUdpSocket::readyRead, this, &RawUdpServer::readPendingDatagrams);

		connect(_inactiveTimer, &QTimer::timeout, this, &RawUdpServer::dataTimeout);
		_inactiveTimer->setSingleShot(true);
		_inactiveTimer->setInterval(3000);
	}

	// apply config
	handleSettingsUpdate(settings::type::RAWUDPSERVER, _config);
}

void RawUdpServer::dataTimeout()
{
	_hyperhdr->setInputInactive(_priority);
}


void RawUdpServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::RAWUDPSERVER)
	{
		const QJsonObject& obj = config.object();

		quint16 port = obj["port"].toInt(5568);

		// port check
		if (_server != nullptr && _server->localPort() != port && _initialized)
		{
			stopServer();
		}

		_port = port;
		_priority = obj["priority"].toInt(109);

		// enable check
		if (obj["enable"].toBool(true))
		{
			startServer();
		}
		else
		{
			if (_initialized)
				_hyperhdr->setInputInactive(_priority);

			_inactiveTimer->stop();

			stopServer();
		}
	}
}

void RawUdpServer::readPendingDatagrams()
{
	while (_server->hasPendingDatagrams()) {
		QNetworkDatagram datagram = _server->receiveDatagram();

		auto dataLen = datagram.data().length();

		if (dataLen % 3 > 0 || dataLen > 1500 || dataLen == 0)
			continue;

		if (_hyperhdr->getPriorityInfo(_priority).componentId != hyperhdr::COMP_RAWUDPSERVER)
			_hyperhdr->registerInput(_priority, hyperhdr::COMP_RAWUDPSERVER, QString("%1").arg(datagram.senderAddress().toString()));

		std::vector<ColorRgb> _ledColors;

		for(int i = 0; i < dataLen;)
		{
			ColorRgb c;
			c.red = datagram.data().at(i++);
			c.green = datagram.data().at(i++);
			c.blue = datagram.data().at(i++);
			_ledColors.push_back(c);
		}

		_hyperhdr->setInput(_priority, _ledColors);

		_inactiveTimer->start();
	}
}

void RawUdpServer::startServer()
{
	if (_server != nullptr && !_initialized)
	{
		if (!_server->bind(QHostAddress::Any, _port))
		{
			Error(_log, "Failed to bind port %d", _port);
		}
		else
		{
			_initialized = true;

			Info(_log, "Started on port %d. Using network interface: %s", _server->localPort(), QSTRING_CSTR(_server->localAddress().toString()));
		}
	}
}

void RawUdpServer::stopServer()
{
	if (_server != nullptr && _initialized)
	{
		// closing
		if (_server != nullptr)
			_server->abort();

		_initialized = false;

		Info(_log, "Stopped");
	}
}
