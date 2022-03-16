#pragma once

// util
#include <utils/Logger.h>
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/Components.h>

// flatbuffer FBS
#include "hyperhdr_reply_generated.h"
#include "hyperhdr_request_generated.h"

class QTcpSocket;
class QTimer;

///
/// @brief Socket (client) of FlatBufferServer
///
class FlatBufferClient : public QObject
{
	Q_OBJECT
public:
	///
	/// @brief Construct the client
	/// @param socket   The socket
	/// @param timeout  The timeout when a client is automatically disconnected and the priority unregistered
	/// @param parent   The parent
	///
	explicit FlatBufferClient(QTcpSocket* socket, int timeout, int hdrToneMappingEnabled, uint8_t* lutBuffer, QObject* parent = nullptr);

signals:
	///
	/// @brief forward register data to HyperHDRDaemon
	///
	void registerGlobalInput(int priority, hyperhdr::Components component, const QString& origin = "FlatBuffer", const QString& owner = "", unsigned smooth_cfg = 0);

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
	void setGlobalInputColor(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, const QString& origin = "FlatBuffer", bool clearEffects = true);

	///
	/// @brief Emits whenever the client disconnected
	///
	void clientDisconnected();

public slots:
	///
	/// @brief Requests a registration from the client
	///
	void registationRequired(int priority);

	///
	/// @brief close the socket and call disconnected()
	///
	void forceClose();

	///
	/// @brief Change HDR tone mapping
	///
	void setHdrToneMappingEnabled(int mode, uint8_t* lutBuffer);

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
	///
	/// @brief Handle the received message
	///
	void handleMessage(const hyperhdrnet::Request* req);

	///
	/// Register new priority
	///
	void handleRegisterCommand(const hyperhdrnet::Register* regReq);

	///
	/// @brief Hande Color message
	///
	void handleColorCommand(const hyperhdrnet::Color* colorReq);

	///
	/// Handle an incoming Image message
	///
	/// @param image the incoming image
	///
	void handleImageCommand(const hyperhdrnet::Image* image);

	///
	/// @brief Handle clear command
	///
	/// @param clear the incoming clear request
	///
	void handleClearCommand(const hyperhdrnet::Clear* clear);

	///
	/// Send handle not implemented
	///
	void handleNotImplemented();

	///
	/// Send a message to the connected client
	///
	void sendMessage();

	///
	/// Send a standard reply indicating success
	///
	void sendSuccessReply();

	///
	/// Send an error message back to the client
	///
	/// @param error String describing the error
	///
	void sendErrorReply(const std::string& error);

private:
	Logger* _log;
	QTcpSocket* _socket;
	const QString _clientAddress;
	QTimer* _timeoutTimer;
	int _timeout;
	int _priority;

	QByteArray _receiveBuffer;

	// Flatbuffers builder
	flatbuffers::FlatBufferBuilder _builder;

	// tone mapping
	int _hdrToneMappingMode;
	uint8_t* _lutBuffer;
};
