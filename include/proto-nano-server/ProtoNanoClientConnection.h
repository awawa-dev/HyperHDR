/* ProtoNanoClientConnection.h
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

#include <message.pb.h>

// util
#include <utils/Logger.h>
#include <image/Image.h>
#include <image/ColorRgb.h>
#include <utils/Components.h>

class QTcpSocket;
class QTimer;

class ProtoNanoClientConnection : public QObject
{
	Q_OBJECT

public:
	explicit ProtoNanoClientConnection(QTcpSocket* socket, int timeout, QObject* parent);

signals:
	void SignalClearGlobalInput(int priority, bool forceClearAll);
	void SignalImportFromProto(int priority, int duration, const Image<ColorRgb>& image, QString clientDescription);
	void SignalSetGlobalColor(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, hyperhdr::Components origin, QString clientDescription);
	void SignalClientConnectionClosed(ProtoNanoClientConnection* client);

public slots:
	void forceClose();

private slots:
	void readyRead();
	void disconnected();

private:
	void handleImageCommand(const proto_ImageRequest& message, Image<ColorRgb>& image);
	void handleClearCommand(const proto_ClearRequest& message);
	void sendMessage(const proto_HyperhdrReply& message);
	void sendSuccessReply();
	void sendErrorReply(const std::string& error);
	void processData(const uint8_t* buffer, uint32_t messageSize);

	static bool readImage(pb_istream_t* stream, const pb_field_t* field, void** arg);
	static bool writeError(pb_ostream_t* stream, const pb_field_iter_t* field, void* const* arg);

private:
	Logger*		_log;
	QTcpSocket*	_socket;	
	const QString _clientAddress;

	QTimer*		_timeoutTimer;
	int			_timeout;
	int			_priority;
	QString		_clientDescription;
	QByteArray	_receiveBuffer;
};
