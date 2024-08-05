/* FlatBuffersServerConnection.h
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

#pragma once

// util
#include <utils/Logger.h>
#include <image/Image.h>
#include <image/ColorRgb.h>
#include <utils/Components.h>
#include <image/MemoryBuffer.h>

class QTcpSocket;
class QLocalSocket;
class QTimer;
namespace FlatBuffersParser
{
	struct FlatbuffersTransientImage;
};

class FlatBuffersServerConnection : public QObject
{
	Q_OBJECT

public:
	explicit FlatBuffersServerConnection(QTcpSocket* socket, QLocalSocket* domain, int timeout, QObject* parent = nullptr);
	~FlatBuffersServerConnection();
	QString getErrorString();

signals:
	void SignalClearGlobalInput(int priority, bool forceClearAll);
	void SignalDirectImageReceivedInTempBuffer(int priority, FlatBuffersParser::FlatbuffersTransientImage* image, int timeout_ms, hyperhdr::Components origin, QString clientDescription);
	void SignalSetGlobalColor(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, hyperhdr::Components origin, QString clientDescription);
	void SignalClientDisconnected(FlatBuffersServerConnection* client);

public slots:
	void forceClose();

private slots:
	void readyRead();
	void disconnected();

private:
	void sendMessage(uint8_t* buffer, size_t size);
	bool initParserLibrary();

private:
	Logger*			_log;
	QTcpSocket*		_socket;
	QLocalSocket*	_domain;
	QString			_clientAddress;
	QTimer*			_timeoutTimer;
	int				_timeout;
	int				_priority;
	QString			_clientDescription;
	int				_mode;

	QString			_error;
	void*			_builder;
	uint32_t		_incomingSize;
	uint32_t		_incomingIndex;
	MemoryBuffer<uint8_t> _incommingBuffer;
};
