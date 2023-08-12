/* ProtoNanoClientConnection.h
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

#pragma once

#include <message.pb.h>

// util
#include <utils/Logger.h>
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/Components.h>


class QTcpSocket;
class QTimer;


///
/// The Connection object created by a ProtoServer when a new connection is established
///
class ProtoNanoClientConnection : public QObject
{
	Q_OBJECT

public:
	///
	/// @brief Construct the client
	/// @param socket   The socket
	/// @param timeout  The timeout when a client is automatically disconnected and the priority unregistered
	/// @param parent   The parent
	///
	explicit ProtoNanoClientConnection(QTcpSocket* socket, int timeout, QObject* parent);

signals:
	///
	/// @brief forward register data to HyperHDRDaemon
	///
	void registerGlobalInput(int priority, hyperhdr::Components component, const QString& origin = "ProtoBuffer", const QString& owner = "", unsigned smooth_cfg = 0);

	///
	/// @brief Forward clear command to HyperHDRDaemon
	///
	void clearGlobalInput(int priority, bool forceClearAll = false);

	///
	/// @brief forward prepared image to HyperHDRDaemon
	///
	bool setGlobalInputImage(int priority, const Image<ColorRgb>& image, int timeout_ms, bool clearEffect = false);

	///
	/// @brief Forward requested color
	///
	void setGlobalInputColor(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, const QString& origin = "ProtoBuffer", bool clearEffects = true);

	///
	/// @brief Emits whenever the client disconnected
	///
	void clientDisconnected();

public slots:
	///
	/// @brief Requests a registration from the client
	///
	void registationRequired(int priority) { if (_priority == priority) _priority = -1; };

	///
	/// @brief close the socket and call disconnected()
	///
	void forceClose();

private slots:
	///
	/// @brief Is called whenever the socket got new data to read
	///
	void readyRead();

	///
	/// @brief Is called when the socket closed the connection, also requests thread exit
	///
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
	QByteArray	_receiveBuffer;
};
