/* FlatBuffersServerConnection.cpp
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


#include <flatbuffers/server/FlatBuffersServerConnection.h>
#include <flatbuffers/parser/FlatBuffersParser.h>
using namespace FlatBuffersParser;

// qt
#include <QTcpSocket>
#include <QLocalSocket>
#include <QHostAddress>
#include <QTimer>

// util includes
#include <utils/FrameDecoder.h>

FlatBuffersServerConnection::FlatBuffersServerConnection(QTcpSocket* socket, QLocalSocket* domain, int timeout, QObject* parent)
	: QObject(parent)
	, _log(Logger::getInstance("FLATBUFSERVER"))
	, _socket(socket)
	, _domain(domain)
	, _clientAddress("@LocalSocket")
	, _timeoutTimer(new QTimer(this))
	, _timeout(timeout * 1000)
	, _priority(140)
	, _mode(0)
	, _builder(nullptr)
	, _incomingSize(0)
	, _incomingIndex(0)
{
	if (!initParserLibrary() || _builder == nullptr)
	{
		_error = "Could not initialize Flatbuffers parser";
		return;
	}

	if (_socket != nullptr)
		_clientAddress = "@" + _socket->peerAddress().toString();

	// timer setup
	_timeoutTimer->setSingleShot(true);
	_timeoutTimer->setInterval(_timeout);
	connect(_timeoutTimer, &QTimer::timeout, this, [&]() {
		Warning(_log, "Timeout (%i) has been reached. Disconnecting", _timeoutTimer->interval());
		forceClose();
	});

	// connect socket signals
	if (_socket != nullptr)
	{
		connect(_socket, &QTcpSocket::readyRead, this, &FlatBuffersServerConnection::readyRead);
		connect(_socket, &QTcpSocket::disconnected, this, &FlatBuffersServerConnection::disconnected);
	}
	else if (_domain != nullptr)
	{
		connect(_domain, &QLocalSocket::readyRead, this, &FlatBuffersServerConnection::readyRead);
		connect(_domain, &QLocalSocket::disconnected, this, &FlatBuffersServerConnection::disconnected);
	}
}

FlatBuffersServerConnection::~FlatBuffersServerConnection()
{
	if (_builder != nullptr)
		releaseFlatbuffersBuilder(_builder);
}

QString FlatBuffersServerConnection::getErrorString()
{
	return _error;
}

bool FlatBuffersServerConnection::initParserLibrary()
{
	_builder = createFlatbuffersBuilder();
	return (_builder != nullptr);
}

void FlatBuffersServerConnection::readyRead()
{
	_timeoutTimer->start();

	while ((_socket != nullptr && _socket->bytesAvailable()) || (_domain != nullptr && _domain->bytesAvailable()))
	{

		if (_incomingSize == 0)
		{
			QByteArray sizeArr;
			if (_socket != nullptr && _socket->bytesAvailable() >= 4)
				sizeArr = _socket->read(4);
			else if (_domain != nullptr && _domain->bytesAvailable() >= 4)
				sizeArr = _domain->read(4);
			else
				return;

			_incomingSize =
				((sizeArr[0] << 24) & 0xFF000000) |
				((sizeArr[1] << 16) & 0x00FF0000) |
				((sizeArr[2] << 8) & 0x0000FF00) |
				((sizeArr[3]) & 0x000000FF);

			if (_incomingSize > 10000000 || _incomingSize == 0)
			{
				Error(_log, "The frame is too large (> 10000000) or too small (0), has %i byte", _incomingSize);
				_incomingSize = 0;
				_incomingIndex = 0;
				QUEUE_CALL_0(this, disconnected);
				return;
			}

			_incomingIndex = 0;
			_incommingBuffer.resize(_incomingSize);
		}

		if (_socket != nullptr)
			_incomingIndex += _socket->read(reinterpret_cast<char*>(_incommingBuffer.data() + _incomingIndex), static_cast<size_t>(_incomingSize) - _incomingIndex);
		else if (_domain != nullptr)
			_incomingIndex += _domain->read(reinterpret_cast<char*>(_incommingBuffer.data() + _incomingIndex), static_cast<size_t>(_incomingSize) - _incomingIndex);


		// check if we can read a header
		if (_incomingSize == _incomingIndex)
		{
			int priority = _priority, duration = 0;
			std::string clientDescription;
			uint8_t* buffer = nullptr;
			size_t bufferSize = 0;
			FlatbuffersTransientImage flatImage{};
			FlatbuffersColor flatColor{};

			auto result = decodeIncomingFlatbuffersFrame(_builder, _incommingBuffer.data(), _incomingSize,
				&priority, &clientDescription, &duration,
				flatImage,
				flatColor,
				&buffer, &bufferSize);

			if (result == FLATBUFFERS_PACKAGE_TYPE::COLOR)
			{
				emit SignalClearGlobalInput(_priority, false);

				if (duration > 0)
				{
					_timeoutTimer->setInterval(duration);
					_timeoutTimer->start();
				}
				else
					_timeoutTimer->stop();


				_mode = result;
				std::vector<ColorRgb> color{ ColorRgb(flatColor.red, flatColor.green, flatColor.blue) };
				if (_clientDescription.isEmpty())
					_clientDescription = QString("Forwarder%1").arg(_clientAddress);
				emit SignalSetGlobalColor(_priority, color, duration, hyperhdr::Components::COMP_FLATBUFSERVER, QString(_clientDescription));

			}
			if (result == FLATBUFFERS_PACKAGE_TYPE::IMAGE)
			{
				if (_mode == FLATBUFFERS_PACKAGE_TYPE::COLOR)
				{
					emit SignalClearGlobalInput(_priority, false);
					_timeoutTimer->setInterval(_timeout);
					_timeoutTimer->start();
				}

				_mode = result;

				emit SignalDirectImageReceivedInTempBuffer(_priority, &flatImage, duration, hyperhdr::Components::COMP_FLATBUFSERVER, _clientDescription);
			}
			if (result == FLATBUFFERS_PACKAGE_TYPE::PRIORITY)
			{
				if (priority < 50 || priority > 250)
				{
					Error(_log, "Register request from client %s contains invalid priority %d. Valid priority for Flatbuffer connections is between 50 and 250.", QSTRING_CSTR(_clientAddress), priority);
				}
				else
				{
					_priority = priority;
					_clientDescription = QString("%1%2").arg(QString::fromStdString(clientDescription)).arg(_clientAddress);
				}
			}
			if (result == FLATBUFFERS_PACKAGE_TYPE::CLEAR)
			{
				emit SignalClearGlobalInput(priority, false);
			}
			if (result == FLATBUFFERS_PACKAGE_TYPE::ERROR)
			{
				Error(_log, "An flatbuffers error has occured");
			}

			if (buffer != nullptr && bufferSize > 0)
			{
				sendMessage(buffer, bufferSize);
			}

			clearFlatbuffersBuilder(_builder);

			_incomingSize = _incomingIndex = 0;
		}
	}
}

void FlatBuffersServerConnection::forceClose()
{
	if (_socket != nullptr)
	{
		_socket->close();
		_socket->deleteLater();
		_socket = nullptr;
	}
	if (_domain != nullptr)
	{
		_domain->close();
		_domain->deleteLater();
		_domain = nullptr;
	}
}

void FlatBuffersServerConnection::disconnected()
{
	Debug(_log, "Socket Closed");

	if (_priority >= 50 && _priority <= 250)
		emit SignalClearGlobalInput(_priority, false);

	emit SignalClientDisconnected(this);
}



void FlatBuffersServerConnection::sendMessage(uint8_t* buffer, size_t size)
{
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

