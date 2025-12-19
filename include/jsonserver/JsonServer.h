#pragma once

#ifndef PCH_ENABLED
	#include <QSet>
#endif

#include <utils/Components.h>
#include <utils/Logger.h>
#include <utils/settings.h>

class QTcpServer;
class QTcpSocket;
class JsonClientConnection;
class BonjourServiceRegister;
class NetOrigin;
class HyperHdrManager;

class JsonServer : public QObject
{
	Q_OBJECT

public:
	JsonServer(std::shared_ptr<NetOrigin> netOrigin, const QJsonDocument& config);
	~JsonServer() override;

	uint16_t getPort() const;

private slots:
	void newConnection();
	void signalClientConnectionClosedHandler(JsonClientConnection* client);

public slots:
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

private:
	QTcpServer* _server;
	QSet<JsonClientConnection*> _openConnections;
	LoggerName _log;

	std::shared_ptr<NetOrigin> _netOrigin;

	uint16_t _port;

	void start();
	void stop();
};
