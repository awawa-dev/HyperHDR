#include "FlatBufferClient.h"

// qt
#include <QTcpSocket>
#include <QLocalSocket>
#include <QHostAddress>
#include <QTimer>

// util includes
#include <utils/FrameDecoder.h>

FlatBufferClient::FlatBufferClient(QTcpSocket* socket, QLocalSocket* domain, int timeout, QObject* parent)
	: QObject(parent)
	, _log(Logger::getInstance("FLATBUFSERVER"))
	, _socket(socket)
	, _domain(domain)
	, _clientAddress("@LocalSocket")
	, _timeoutTimer(new QTimer(this))
	, _timeout(timeout * 1000)
	, _priority(140)
{
	if (_socket != nullptr)
		_clientAddress = "@" + _socket->peerAddress().toString();

	// timer setup
	_timeoutTimer->setSingleShot(true);
	_timeoutTimer->setInterval(_timeout);
	connect(_timeoutTimer, &QTimer::timeout, this, &FlatBufferClient::forceClose);

	// connect socket signals
	if (_socket != nullptr)
	{
		connect(_socket, &QTcpSocket::readyRead, this, &FlatBufferClient::readyRead);
		connect(_socket, &QTcpSocket::disconnected, this, &FlatBufferClient::disconnected);
	}
	else if (_domain != nullptr)
	{
		connect(_domain, &QLocalSocket::readyRead, this, &FlatBufferClient::readyRead);
		connect(_domain, &QLocalSocket::disconnected, this, &FlatBufferClient::disconnected);
	}
}

void FlatBufferClient::readyRead()
{
	_timeoutTimer->start();

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

		const auto* msgData = reinterpret_cast<const uint8_t*>(msg.constData());
		flatbuffers::Verifier verifier(msgData, messageSize);

		if (hyperhdrnet::VerifyRequestBuffer(verifier))
		{
			auto message = hyperhdrnet::GetRequest(msgData);
			handleMessage(message);
			continue;
		}
		sendErrorReply("Unable to parse message");
	}
}

void FlatBufferClient::forceClose()
{
	if (_socket != nullptr)
		_socket->close();
	if (_domain != nullptr)
		_domain->close();
}

void FlatBufferClient::disconnected()
{
	Debug(_log, "Socket Closed");

	if (_socket != nullptr)
		_socket->deleteLater();
	if (_domain != nullptr)
		_domain->deleteLater();

	if (_priority >= 50 && _priority <= 250)
		emit SignalClearGlobalInput(_priority, false);

	emit SignalClientDisconnected(this);
}

void FlatBufferClient::handleMessage(const hyperhdrnet::Request* req)
{
	const void* reqPtr;
	if ((reqPtr = req->command_as_Color()) != nullptr) {
		handleColorCommand(static_cast<const hyperhdrnet::Color*>(reqPtr));
	}
	else if ((reqPtr = req->command_as_Image()) != nullptr) {
		handleImageCommand(static_cast<const hyperhdrnet::Image*>(reqPtr));
	}
	else if ((reqPtr = req->command_as_Clear()) != nullptr) {
		handleClearCommand(static_cast<const hyperhdrnet::Clear*>(reqPtr));
	}
	else if ((reqPtr = req->command_as_Register()) != nullptr) {
		handleRegisterCommand(static_cast<const hyperhdrnet::Register*>(reqPtr));
	}
	else {
		sendErrorReply("Received invalid packet.");
	}
}

void FlatBufferClient::handleColorCommand(const hyperhdrnet::Color* colorReq)
{
	// extract parameters
	const int32_t rgbData = colorReq->data();
	std::vector<ColorRgb> color{ ColorRgb{ uint8_t((rgbData >> 16) & 0xff), uint8_t((rgbData >> 8) & 0xff), uint8_t(rgbData & 0xff) } };

	// set output
	emit SignalSetGlobalColor(_priority, color, colorReq->duration(), hyperhdr::Components::COMP_FLATBUFSERVER, _clientDescription);

	// send reply
	sendSuccessReply();
}

void FlatBufferClient::handleRegisterCommand(const hyperhdrnet::Register* regReq)
{
	if (regReq->priority() < 50 || regReq->priority() > 250)
	{
		Error(_log, "Register request from client %s contains invalid priority %d. Valid priority for Flatbuffer connections is between 50 and 250.", QSTRING_CSTR(_clientAddress), regReq->priority());
		sendErrorReply("The priority " + std::to_string(regReq->priority()) + " is not in the priority range between 50 and 250.");
		return;
	}

	_priority = regReq->priority();
	_clientDescription = regReq->origin()->c_str() + _clientAddress;

	auto reply = hyperhdrnet::CreateReplyDirect(_builder, nullptr, -1, _priority);
	_builder.Finish(reply);

	// send reply
	sendMessage();

	_builder.Clear();
}

void FlatBufferClient::handleImageCommand(const hyperhdrnet::Image* image)
{
	// extract parameters
	int duration = image->duration();

	const void* reqPtr;
	if ((reqPtr = image->data_as_RawImage()) != nullptr)
	{
		const auto* img = static_cast<const hyperhdrnet::RawImage*>(reqPtr);
		const auto& imageData = img->data();
		const int width = img->width();
		const int height = img->height();

		if ((int)imageData->size() != width * height * 3)
		{
			sendErrorReply("Size of image data does not match with the width and height");
			return;
		}

		Image<ColorRgb> imageDest(width, height);
		memmove(imageDest.rawMem(), imageData->data(), imageData->size());

		emit SignalImageReceived(_priority, imageDest, duration, hyperhdr::Components::COMP_FLATBUFSERVER, _clientDescription);
	}

	// send reply
	sendSuccessReply();
}


void FlatBufferClient::handleClearCommand(const hyperhdrnet::Clear* clear)
{
	// extract parameters
	const int priority = clear->priority();	

	emit SignalClearGlobalInput(priority, false);

	sendSuccessReply();
}

void FlatBufferClient::handleNotImplemented()
{
	sendErrorReply("Command not implemented");
}

void FlatBufferClient::sendMessage()
{
	auto size = _builder.GetSize();
	const uint8_t* buffer = _builder.GetBufferPointer();
	uint8_t sizeData[] = { uint8_t(size >> 24), uint8_t(size >> 16), uint8_t(size >> 8), uint8_t(size) };

	if (_socket != nullptr)
	{
		_socket->write((const char*)sizeData, sizeof(sizeData));
		_socket->write((const char*)buffer, size);
		_socket->flush();
	}
	else if (_domain != nullptr)
	{
		_domain->write((const char*)sizeData, sizeof(sizeData));
		_domain->write((const char*)buffer, size);
		_domain->flush();
	}
}

void FlatBufferClient::sendSuccessReply()
{
	auto reply = hyperhdrnet::CreateReplyDirect(_builder);
	_builder.Finish(reply);

	// send reply
	sendMessage();

	_builder.Clear();
}

void FlatBufferClient::sendErrorReply(const std::string& error)
{
	// create reply
	auto reply = hyperhdrnet::CreateReplyDirect(_builder, error.c_str());
	_builder.Finish(reply);

	// send reply
	sendMessage();

	_builder.Clear();
}
