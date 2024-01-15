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
class QLocalSocket;
class QTimer;

class FlatBufferClient : public QObject
{
	Q_OBJECT

public:
	explicit FlatBufferClient(QTcpSocket* socket, QLocalSocket* domain, int timeout, QObject* parent = nullptr);

signals:
	void SignalClearGlobalInput(int priority, bool forceClearAll);
	void SignalImageReceived(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin, QString clientDescription);
	void SignalSetGlobalColor(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, hyperhdr::Components origin, QString clientDescription);
	void SignalClientDisconnected(FlatBufferClient* client);

public slots:
	void forceClose();

private slots:
	void readyRead();
	void disconnected();

private:
	void handleMessage(const hyperhdrnet::Request* req);
	void handleRegisterCommand(const hyperhdrnet::Register* regReq);
	void handleColorCommand(const hyperhdrnet::Color* colorReq);
	void handleImageCommand(const hyperhdrnet::Image* image);
	void handleClearCommand(const hyperhdrnet::Clear* clear);
	void handleNotImplemented();
	void sendMessage();
	void sendSuccessReply();
	void sendErrorReply(const std::string& error);

private:
	Logger*			_log;
	QTcpSocket*		_socket;
	QLocalSocket*	_domain;
	QString			_clientAddress;
	QTimer*			_timeoutTimer;
	int				_timeout;
	int				_priority;
	QString			_clientDescription;

	QByteArray		_receiveBuffer;

	flatbuffers::FlatBufferBuilder _builder;
};
