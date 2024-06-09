/* FlatBuffersClient.cpp
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

// stl includes
#include <stdexcept>

// flatbuffer includes
#include <flatbuffers/client/FlatBuffersClient.h>

// flatbuffer FBS
#include <flatbuffers/parser/FlatBuffersParser.h>

using namespace FlatBuffersParser;

FlatBuffersClient::FlatBuffersClient(QObject* parent, const QString& origin, const QString& address, int priority, bool skipReply)
	: QObject(parent)
	, _socket((address == HYPERHDR_DOMAIN_SERVER) ? nullptr : new QTcpSocket(this))
	, _domain((address == HYPERHDR_DOMAIN_SERVER) ? new QLocalSocket(this) : nullptr)
	, _origin(origin)
	, _priority(priority)
	, _prevSocketState(QAbstractSocket::UnconnectedState)
	, _prevLocalState(QLocalSocket::UnconnectedState)
	, _log(Logger::getInstance("FLATBUFCONN"))
	, _builder(nullptr)
	, _registered(false)
	, _sent(false)
	, _lastSendImage(0)
{
	if (!initParserLibrary() || _builder == nullptr)
	{
		_error = "Could not initialize Flatbuffers parser";
		return;
	}

	if (_socket == nullptr)
		Info(_log, "Connection using local domain socket. Ignoring port.");
	else
		Info(_log, "Connection using TCP socket (address != %s)", QSTRING_CSTR(HYPERHDR_DOMAIN_SERVER));

	if (_socket != nullptr)
	{
		QStringList parts = address.split(":");
		if (parts.size() != 2)
		{
			_error = QString("FlatBuffersClient: Unable to parse address (%1)").arg(address);
			return;
		}
		_host = parts[0];

		bool ok;
		_port = parts[1].toUShort(&ok);
		if (!ok)
		{
			_error = QString("FlatBuffersClient: Unable to parse the port (%1)").arg(parts[1]);
			return;
		}
	}
	else
	{
		_host = address;
		_port = 0;
	}

	if (!skipReply)
	{
		if (_socket != nullptr)
		{
			connect(_socket, &QTcpSocket::readyRead, this, &FlatBuffersClient::readData, Qt::UniqueConnection);
			connect(_socket, &QTcpSocket::connected, this, [&]() { setRegister(_origin, _priority); });
		}
		else if (_domain != nullptr)
		{
			connect(_domain, &QLocalSocket::readyRead, this, &FlatBuffersClient::readData, Qt::UniqueConnection);
			connect(_domain, &QLocalSocket::connected, this, [&]() { setRegister(_origin, _priority); });
		}
	}

	// init connect
	if (_socket == nullptr)
		Info(_log, "Connecting to HyperHDR local domain: %s", _host.toStdString().c_str());
	else
		Info(_log, "Connecting to HyperHDR: %s:%d", _host.toStdString().c_str(), _port);

	connectToHost();

	// start the connection timer
	_timer.setInterval(5000);

	connect(&_timer, &QTimer::timeout, this, &FlatBuffersClient::connectToHost);
	_timer.start();

	connect(this, &FlatBuffersClient::SignalImageToSend, this, &FlatBuffersClient::sendImage);
	connect(this, &FlatBuffersClient::SignalSetColor, this, &FlatBuffersClient::setColorHandler);
}

QString FlatBuffersClient::getErrorString()
{
	return _error;
}

void FlatBuffersClient::setColorHandler(ColorRgb color, int duration)
{
	setColor(color, _priority, duration);
}

bool FlatBuffersClient::initParserLibrary()
{
	_builder = createFlatbuffersBuilder();
	return (_builder != nullptr);
}

FlatBuffersClient::~FlatBuffersClient()
{
	_timer.stop();

	if (_socket != nullptr)
		_socket->close();
	if (_domain != nullptr)
		_domain->close();

	if (_builder != nullptr)
		releaseFlatbuffersBuilder(_builder);
}

void FlatBuffersClient::readData()
{
	if (_socket != nullptr)
		_receiveBuffer += _socket->readAll();
	else if (_domain != nullptr)
		_receiveBuffer += _domain->readAll();

	// check if we can read a header
	while (_receiveBuffer.size() >= 4)
	{
		uint32_t messageSize =
			((_receiveBuffer[0] << 24) & 0xFF000000) |
			((_receiveBuffer[1] << 16) & 0x00FF0000) |
			((_receiveBuffer[2] << 8) & 0x0000FF00) |
			((_receiveBuffer[3]) & 0x000000FF);

		// check if we can read a complete message
		if ((uint32_t)_receiveBuffer.size() < messageSize + 4) return;

		// extract message only and remove header + msg from buffer :: QByteArray::remove() does not return the removed data
		const QByteArray msg = _receiveBuffer.mid(4, messageSize);
		_receiveBuffer.remove(0, messageSize + 4);

		if (!verifyFlatbuffersReplyBuffer(reinterpret_cast<const uint8_t*>(msg.data()), messageSize, &_sent, &_registered, &_priority))
			Error(_log, "Unable to parse reply");
	}
}

void FlatBuffersClient::setSkipReply(bool skip)
{
	if (_socket != nullptr)
	{
		if (skip)
			disconnect(_socket, &QTcpSocket::readyRead, 0, 0);
		else
			connect(_socket, &QTcpSocket::readyRead, this, &FlatBuffersClient::readData, Qt::UniqueConnection);
	}
	if (_domain != nullptr)
	{
		if (skip)
			disconnect(_domain, &QLocalSocket::readyRead, 0, 0);
		else
			connect(_domain, &QLocalSocket::readyRead, this, &FlatBuffersClient::readData, Qt::UniqueConnection);
	}
}

void FlatBuffersClient::setRegister(const QString& origin, int priority)
{
	uint8_t* outputbuffer = nullptr;
	size_t outputbufferSize = 0;

	encodeRegisterPriorityIntoFlatbuffers(_builder, priority, QSTRING_CSTR(origin), &outputbuffer, &outputbufferSize);


	const uint8_t header[] = {
		uint8_t((outputbufferSize >> 24) & 0xFF),
		uint8_t((outputbufferSize >> 16) & 0xFF),
		uint8_t((outputbufferSize >> 8) & 0xFF),
		uint8_t((outputbufferSize) & 0xFF) };

	// write message
	if (_socket != nullptr)
	{
		_socket->write(reinterpret_cast<const char*>(header), 4);
		_socket->write(reinterpret_cast<const char*>(outputbuffer), outputbufferSize);
		_socket->flush();
	}
	else if (_domain != nullptr)
	{
		_domain->write(reinterpret_cast<const char*>(header), 4);
		_domain->write(reinterpret_cast<const char*>(outputbuffer), outputbufferSize);
		_domain->flush();
	}
	clearFlatbuffersBuilder(_builder);
}

void FlatBuffersClient::setColor(const ColorRgb& color, int priority, int duration)
{
	uint8_t* outputbuffer = nullptr;
	size_t outputbufferSize = 0;
	encodeColorIntoFlatbuffers(_builder, color.red, color.green, color.blue, priority, duration, &outputbuffer, &outputbufferSize);
	sendMessage(outputbuffer, outputbufferSize);
	clearFlatbuffersBuilder(_builder);
}

void FlatBuffersClient::sendImage(const Image<ColorRgb>& image)
{
	auto current = InternalClock::now();
	auto outOfTime = (current - _lastSendImage);

	if (_socket != nullptr && _socket->state() != QAbstractSocket::ConnectedState)
		return;

	if (_domain != nullptr && _domain->state() != QLocalSocket::ConnectedState)
		return;

	if (_sent && outOfTime < 1000)
		return;

	if (_lastSendImage > 0)
	{
		if (outOfTime >= 1000)
			Warning(_log, "Critical low network performance for Flatbuffers stream (frame sent time: %ims)", int(outOfTime));
		else if (outOfTime > 300)
			Warning(_log, "Poor network performance for Flatbuffers stream (frame sent time: %ims)", int(outOfTime));
	}

	_sent = true;
	_lastSendImage = current;

	// encode and send
	uint8_t* outputbuffer = nullptr;
	size_t outputbufferSize = 0;
	encodeImageIntoFlatbuffers(_builder, image.rawMem(), image.size(), image.width(), image.height(), &outputbuffer, &outputbufferSize);
	sendMessage(outputbuffer, outputbufferSize);
	clearFlatbuffersBuilder(_builder);
}

void FlatBuffersClient::clear(int priority)
{
	// encode and send
	uint8_t* outputbuffer = nullptr;
	size_t outputbufferSize = 0;
	encodeClearPriorityIntoFlatbuffers(_builder, priority, &outputbuffer, &outputbufferSize);
	sendMessage(outputbuffer, outputbufferSize);
	clearFlatbuffersBuilder(_builder);
}

void FlatBuffersClient::clearAll()
{
	clear(-1);
}

void FlatBuffersClient::connectToHost()
{
	// try connection only when
	if (_socket != nullptr && _socket->state() == QAbstractSocket::UnconnectedState)
		_socket->connectToHost(_host, _port);
	else if (_domain != nullptr && _domain->state() == QLocalSocket::UnconnectedState)
		_domain->connectToServer(_host);
}

void FlatBuffersClient::sendMessage(const uint8_t* buffer, uint32_t size)
{
	// print out connection message only when state is changed
	if (_socket != nullptr && _socket->state() != _prevSocketState)
	{
		_registered = false;
		switch (_socket->state())
		{
			case QAbstractSocket::UnconnectedState:
				Info(_log, "No connection to HyperHDR: %s:%d", _host.toStdString().c_str(), _port);
				break;
			case QAbstractSocket::ConnectedState:
				Info(_log, "Connected to HyperHDR: %s:%d", _host.toStdString().c_str(), _port);
				break;
			default:
				Debug(_log, "Connecting to HyperHDR: %s:%d", _host.toStdString().c_str(), _port);
				break;
		}
		_prevSocketState = _socket->state();
	}
	if (_socket != nullptr && _socket->state() != QAbstractSocket::ConnectedState)
		return;

	if (_domain != nullptr && _domain->state() != _prevLocalState)
	{
		_registered = false;
		switch (_domain->state())
		{
			case QLocalSocket::UnconnectedState:
				Info(_log, "No connection to HyperHDR domain: %s", _host.toStdString().c_str());
				break;
			case QLocalSocket::ConnectedState:
				Info(_log, "Connected to HyperHDR domain: %s", _host.toStdString().c_str());
				break;
			default:
				Debug(_log, "Connecting to HyperHDR domain: %s", _host.toStdString().c_str());
				break;
		}
		_prevLocalState = _domain->state();
	}
	if (_domain != nullptr && _domain->state() != QLocalSocket::ConnectedState)
		return;

	if (!_registered)
	{
		setRegister(_origin, _priority);
		return;
	}

	const uint8_t header[] = {
		uint8_t((size >> 24) & 0xFF),
		uint8_t((size >> 16) & 0xFF),
		uint8_t((size >> 8) & 0xFF),
		uint8_t((size) & 0xFF) };

	// write message
	if (_socket != nullptr)
	{
		_socket->write(reinterpret_cast<const char*>(header), 4);
		_socket->write(reinterpret_cast<const char*>(buffer), size);
		_socket->flush();
	}
	else if (_domain != nullptr)
	{
		_domain->write(reinterpret_cast<const char*>(header), 4);
		_domain->write(reinterpret_cast<const char*>(buffer), size);
		_domain->flush();
	}
}

