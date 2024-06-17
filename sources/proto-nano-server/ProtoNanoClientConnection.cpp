/* ProtoNanoClientConnection.cpp
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

#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QThread>

#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>
#include <ProtoNanoClientConnection.h>
#include <flatbuffers/server/FlatBuffersServer.h>

ProtoNanoClientConnection::ProtoNanoClientConnection(QTcpSocket* socket, int timeout, QObject* parent)
	: QObject(parent)
	, _log(Logger::getInstance("PROTOSERVER"))
	, _socket(socket)
	, _clientAddress(socket->peerAddress().toString())
	, _timeoutTimer(new QTimer(this))
	, _timeout(timeout * 1000)
	, _priority(145)
{
	// timer setup
	_timeoutTimer->setSingleShot(true);
	_timeoutTimer->setInterval(_timeout);
	connect(_timeoutTimer, &QTimer::timeout, this, &ProtoNanoClientConnection::forceClose);

	// connect socket signals
	connect(_socket, &QTcpSocket::readyRead, this, &ProtoNanoClientConnection::readyRead);
	connect(_socket, &QTcpSocket::disconnected, this, &ProtoNanoClientConnection::disconnected);
}

void ProtoNanoClientConnection::readyRead()
{
	_receiveBuffer += _socket->readAll();

	// check if we can read a message size
	if (_receiveBuffer.size() <= 4)
	{
		return;
	}

	// read the message size
	uint32_t messageSize =
		((_receiveBuffer[0] << 24) & 0xFF000000) |
		((_receiveBuffer[1] << 16) & 0x00FF0000) |
		((_receiveBuffer[2] << 8) & 0x0000FF00) |
		((_receiveBuffer[3]) & 0x000000FF);

	// check if we can read a complete message
	if ((uint32_t)_receiveBuffer.size() < messageSize + 4)
	{
		return;
	}

	processData((const uint8_t*)(_receiveBuffer.data() + 4), messageSize);

	_receiveBuffer = _receiveBuffer.mid(messageSize + 4);
}

bool ProtoNanoClientConnection::readImage(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
	int sizeRgb = (int)((stream->bytes_left) / 3);
	Image<ColorRgb>* image = (Image<ColorRgb>*) * arg;

	image->resize(sizeRgb, 1);

	if (!pb_read(stream, image->rawMem(), sizeRgb * 3))
		return false;

	return true;
}

void ProtoNanoClientConnection::processData(const uint8_t* buffer, uint32_t messageSize)
{
	pb_istream_t stream = pb_istream_from_buffer(buffer, messageSize);

	proto_HyperhdrRequest mainMessage = proto_HyperhdrRequest_init_zero;
	pb_extension_t imageExt = pb_extension_init_zero;
	pb_extension_t clearExt = pb_extension_init_zero;
	proto_ImageRequest imageReq = proto_ImageRequest_init_zero;
	proto_ClearRequest clearReq = proto_ClearRequest_init_zero;

	Image<ColorRgb> image;

	imageReq.imagedata.funcs.decode = &ProtoNanoClientConnection::readImage;
	imageReq.imagedata.arg = &image;

	mainMessage.extensions = &imageExt;
	imageExt.type = &proto_ImageRequest_imageRequest;
	imageExt.dest = &imageReq;
	imageExt.next = &clearExt;
	clearExt.type = &proto_ClearRequest_clearRequest;
	clearExt.dest = &clearReq;
	clearExt.next = nullptr;

	bool status = pb_decode(&stream, proto_HyperhdrRequest_fields, &mainMessage);

	if (status)
	{
		status = false;

		if (imageExt.found)
		{
			status = true;
			handleImageCommand(imageReq, image);
		}

		if (clearExt.found)
		{
			status = true;
			handleClearCommand(clearReq);
		}

		if (!status)
			Warning(_log, "Unsupported request");
	}
	else
		Error(_log, "Error while decoding the message");
}

void ProtoNanoClientConnection::forceClose()
{
	_socket->close();
}

void ProtoNanoClientConnection::disconnected()
{
	Debug(_log, "Socket Closed");
	_socket->deleteLater();
	emit SignalClearGlobalInput(_priority, false);
	emit SignalClientConnectionClosed(this);
}

void ProtoNanoClientConnection::handleImageCommand(const proto_ImageRequest& message, Image<ColorRgb>& image)
{
	// extract parameters
	int priority = message.priority;
	int duration = message.has_duration ? message.duration : -1;
	int width = message.imagewidth;
	int height = message.imageheight;

	if (priority < 50 || priority > 250)
	{
		sendErrorReply("The priority " + std::to_string(priority) + " is not in the valid priority range between 50 and 250.");
		Error(_log, "The priority %d is not in the proto-connection range between 50 and 250.", priority);
		return;
	}

	// make sure the prio is registered before setInput()
	if (priority != _priority)
	{
		emit SignalClearGlobalInput(_priority, false);
		_clientDescription = "Proto@" + _clientAddress;
		_priority = priority;
	}

	// check consistency of the size of the received data
	if ((int)image.size() != width * height * 3)
	{
		sendErrorReply("Size of image data does not match with the width and height");
		Error(_log, "Size of image data does not match with the width and height");
		return;
	}

	// must resize
	image.resize(width, height);	

	emit SignalImportFromProto(_priority, duration, image, _clientDescription);

	// send reply
	sendSuccessReply();
}


void ProtoNanoClientConnection::handleClearCommand(const proto_ClearRequest& message)
{
	// extract parameters
	int priority = message.priority;

	// clear priority
	emit SignalClearGlobalInput(priority, false);

	// send reply
	sendSuccessReply();
}


void ProtoNanoClientConnection::sendMessage(const proto_HyperhdrReply& message)
{
	uint8_t buffer[256];
	uint32_t size;
	bool status;

	pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
	status = pb_encode(&stream, proto_HyperhdrReply_fields, &message);
	size = (uint32_t)(stream.bytes_written);

	if (status)
	{
		uint8_t sizeData[] = { uint8_t(size >> 24), uint8_t(size >> 16), uint8_t(size >> 8), uint8_t(size) };
		_socket->write((const char*)sizeData, sizeof(sizeData));
		_socket->write((const char*)&buffer, size);
		_socket->flush();
	}
	else
		Warning(_log, "Failed to send reply");
}

void ProtoNanoClientConnection::sendSuccessReply()
{
	// create reply
	proto_HyperhdrReply reply = proto_HyperhdrReply_init_zero;
	reply.type = _proto_HyperhdrReply_Type::proto_HyperhdrReply_Type_REPLY;
	reply.success = true;
	reply.has_success = true;

	// send reply
	sendMessage(reply);
}

bool ProtoNanoClientConnection::writeError(pb_ostream_t* stream, const pb_field_iter_t* field, void* const* arg)
{
	const char* str = (const char*) *arg;

	if (!pb_encode_tag_for_field(stream, field))
		return false;

	return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}

void ProtoNanoClientConnection::sendErrorReply(const std::string& error)
{
	// create reply
	proto_HyperhdrReply reply = proto_HyperhdrReply_init_zero;
	reply.type = _proto_HyperhdrReply_Type::proto_HyperhdrReply_Type_REPLY;
	reply.success = false;
	reply.has_success = true;
	reply.error.arg = (char*) error.c_str();
	reply.error.funcs.encode = &ProtoNanoClientConnection::writeError;

	// send reply
	sendMessage(reply);
}
