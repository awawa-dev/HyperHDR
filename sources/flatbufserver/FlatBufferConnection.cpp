// stl includes
#include <stdexcept>

// Qt includes
#include <QRgb>

// flatbuffer includes
#include <flatbufserver/FlatBufferConnection.h>

// flatbuffer FBS
#include "hyperhdr_reply_generated.h"
#include "hyperhdr_request_generated.h"

FlatBufferConnection::FlatBufferConnection(const QString& origin, const QString& address, int priority, bool skipReply)
	: _socket((address == HYPERHDR_DOMAIN_SERVER) ? nullptr : new QTcpSocket())
	, _domain((address == HYPERHDR_DOMAIN_SERVER) ? new QLocalSocket() : nullptr)
	, _origin(origin)
	, _priority(priority)
	, _prevSocketState(QAbstractSocket::UnconnectedState)
	, _prevLocalState(QLocalSocket::UnconnectedState)
	, _log(Logger::getInstance("FLATBUFCONN"))
	, _registered(false)
	, _sent(false)
	, _lastSendImage(0)
{
	if (_socket == nullptr)
		Info(_log, "Connection using local domain socket. Ignoring port.");
	else
		Info(_log, "Connection using TCP socket (address != %s)", QSTRING_CSTR(HYPERHDR_DOMAIN_SERVER));

	if (_socket != nullptr)
	{
		QStringList parts = address.split(":");
		if (parts.size() != 2)
		{
			throw std::runtime_error(QString("FLATBUFCONNECTION ERROR: Unable to parse address (%1)").arg(address).toStdString());
		}
		_host = parts[0];

		bool ok;
		_port = parts[1].toUShort(&ok);
		if (!ok)
		{
			throw std::runtime_error(QString("FLATBUFCONNECTION ERROR: Unable to parse the port (%1)").arg(parts[1]).toStdString());
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
			connect(_socket, &QTcpSocket::readyRead, this, &FlatBufferConnection::readData, Qt::UniqueConnection);
		else if (_domain != nullptr)
			connect(_domain, &QLocalSocket::readyRead, this, &FlatBufferConnection::readData, Qt::UniqueConnection);
	}

	// init connect
	if (_socket == nullptr)
		Info(_log, "Connecting to HyperHDR local domain: %s", _host.toStdString().c_str());
	else
		Info(_log, "Connecting to HyperHDR: %s:%d", _host.toStdString().c_str(), _port);

	connectToHost();

	// start the connection timer
	_timer.setInterval(5000);

	connect(&_timer, &QTimer::timeout, this, &FlatBufferConnection::connectToHost);
	_timer.start();

	connect(this, &FlatBufferConnection::onImage, this, &FlatBufferConnection::setImage);
}

FlatBufferConnection::~FlatBufferConnection()
{
	_timer.stop();

	if (_socket != nullptr)
		_socket->close();
	if (_domain != nullptr)
		_domain->close();
}

void FlatBufferConnection::readData()
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

		const uint8_t* msgData = reinterpret_cast<const uint8_t*>(msg.constData());
		flatbuffers::Verifier verifier(msgData, messageSize);

		if (hyperhdrnet::VerifyReplyBuffer(verifier))
		{
			parseReply(hyperhdrnet::GetReply(msgData));
			continue;
		}
		Error(_log, "Unable to parse reply");
	}
}

void FlatBufferConnection::setSkipReply(bool skip)
{
	if (_socket != nullptr)
	{
		if (skip)
			disconnect(_socket, &QTcpSocket::readyRead, 0, 0);
		else
			connect(_socket, &QTcpSocket::readyRead, this, &FlatBufferConnection::readData, Qt::UniqueConnection);
	}
	if (_domain != nullptr)
	{
		if (skip)
			disconnect(_domain, &QLocalSocket::readyRead, 0, 0);
		else
			connect(_domain, &QLocalSocket::readyRead, this, &FlatBufferConnection::readData, Qt::UniqueConnection);
	}
}

void FlatBufferConnection::setRegister(const QString& origin, int priority)
{
	auto registerReq = hyperhdrnet::CreateRegister(_builder, _builder.CreateString(QSTRING_CSTR(origin)), priority);
	auto req = hyperhdrnet::CreateRequest(_builder, hyperhdrnet::Command_Register, registerReq.Union());

	_builder.Finish(req);
	uint32_t size = _builder.GetSize();
	const uint8_t header[] = {
		uint8_t((size >> 24) & 0xFF),
		uint8_t((size >> 16) & 0xFF),
		uint8_t((size >> 8) & 0xFF),
		uint8_t((size) & 0xFF) };

	// write message
	if (_socket != nullptr)
	{
		_socket->write(reinterpret_cast<const char*>(header), 4);
		_socket->write(reinterpret_cast<const char*>(_builder.GetBufferPointer()), size);
		_socket->flush();
	}
	else if (_domain != nullptr)
	{
		_domain->write(reinterpret_cast<const char*>(header), 4);
		_domain->write(reinterpret_cast<const char*>(_builder.GetBufferPointer()), size);
		_domain->flush();
	}
	_builder.Clear();
}

void FlatBufferConnection::setColor(const ColorRgb& color, int priority, int duration)
{
	auto colorReq = hyperhdrnet::CreateColor(_builder, (color.red << 16) | (color.green << 8) | color.blue, duration);
	auto req = hyperhdrnet::CreateRequest(_builder, hyperhdrnet::Command_Color, colorReq.Union());

	_builder.Finish(req);
	sendMessage(_builder.GetBufferPointer(), _builder.GetSize());
	_builder.Clear();
}

void FlatBufferConnection::setImage(const Image<ColorRgb>& image)
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

	auto imgData = _builder.CreateVector(image.rawMem(), image.size());
	auto rawImg = hyperhdrnet::CreateRawImage(_builder, imgData, image.width(), image.height());
	auto imageReq = hyperhdrnet::CreateImage(_builder, hyperhdrnet::ImageType_RawImage, rawImg.Union(), -1);
	auto req = hyperhdrnet::CreateRequest(_builder, hyperhdrnet::Command_Image, imageReq.Union());

	_builder.Finish(req);
	sendMessage(_builder.GetBufferPointer(), _builder.GetSize());
	_builder.Clear();
}

void FlatBufferConnection::clear(int priority)
{
	auto clearReq = hyperhdrnet::CreateClear(_builder, priority);
	auto req = hyperhdrnet::CreateRequest(_builder, hyperhdrnet::Command_Clear, clearReq.Union());

	_builder.Finish(req);
	sendMessage(_builder.GetBufferPointer(), _builder.GetSize());
	_builder.Clear();
}

void FlatBufferConnection::clearAll()
{
	clear(-1);
}

void FlatBufferConnection::connectToHost()
{
	// try connection only when
	if (_socket != nullptr && _socket->state() == QAbstractSocket::UnconnectedState)
		_socket->connectToHost(_host, _port);
	else if (_domain != nullptr && _domain->state() == QLocalSocket::UnconnectedState)
		_domain->connectToServer(_host);
}

void FlatBufferConnection::sendMessage(const uint8_t* buffer, uint32_t size)
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

bool FlatBufferConnection::parseReply(const hyperhdrnet::Reply* reply)
{
	_sent = false;

	if (!reply->error())
	{
		// no error set must be a success or registered or video
		//const auto videoMode =
		reply->video();
		const auto registered = reply->registered();
		/*if (videoMode != -1) {
			// We got a video reply.
			emit setVideoMode(static_cast<VideoMode>(videoMode));
			return true;
		}*/

		// We got a registered reply.
		if (registered == -1 || registered != _priority)
			_registered = false;
		else
			_registered = true;

		return true;
	}
	else
		throw std::runtime_error(reply->error()->str());

	return false;
}
