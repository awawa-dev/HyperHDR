#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QByteArray>
	#include <QJsonObject>
#endif

#include <utils/Logger.h>

class HyperAPI;
class QTcpSocket;
class HyperHdrManager;

class JsonClientConnection : public QObject
{
	Q_OBJECT

public:
	JsonClientConnection(QTcpSocket* socket, bool localConnection);

signals:
	void SignalClientConnectionClosed(JsonClientConnection* client);

public slots:
	qint64 sendMessage(QJsonObject);

private slots:
	void readRequest();
	void disconnected();

private:
	QTcpSocket* _socket;
	HyperAPI* _hyperAPI;
	QByteArray _receiveBuffer;
	Logger* _log;
};
