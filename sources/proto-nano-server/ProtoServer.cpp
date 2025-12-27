#include <ProtoNanoClientConnection.h>
#include <ProtoServer.h>

// util
#include <utils/NetOrigin.h>
#include <utils/GlobalSignals.h>

// qt
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

ProtoServer::ProtoServer(std::shared_ptr<NetOrigin> netOrigin, const QJsonDocument& config, QObject* parent)
	: QObject(parent)
	, _server(new QTcpServer(this))
	, _netOrigin(netOrigin)
	, _log("PROTOSERVER")
	, _timeout(5000)
	, _port(19445)
	, _config(config)
{

}

ProtoServer::~ProtoServer()
{
	stopServer();
	Debug(_log, "ProtoServer instance is closed");
}

void ProtoServer::initServer()
{
	connect(_server, &QTcpServer::newConnection, this, &ProtoServer::newConnection);
	handleSettingsUpdate(settings::type::PROTOSERVER, _config);
}

void ProtoServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::PROTOSERVER)
	{
		const QJsonObject& obj = config.object();

		quint16 port = obj["port"].toInt(19445);

		// port check
		if (_server->serverPort() != port)
		{
			stopServer();
			_port = port;
		}

		// new timeout just for new connections
		_timeout = obj["timeout"].toInt(5000);
		// enable check
		obj["enable"].toBool(true) ? startServer() : stopServer();
	}
}

void ProtoServer::newConnection()
{
	while (_server->hasPendingConnections())
	{
		if (QTcpSocket* socket = _server->nextPendingConnection())
		{
			if (_netOrigin->accessAllowed(socket->peerAddress(), socket->localAddress()))
			{
				Debug(_log, "New connection from {:s}", (socket->peerAddress().toString()));
				ProtoNanoClientConnection* client = new ProtoNanoClientConnection(socket, _timeout, this);
				// internal
				connect(client, &ProtoNanoClientConnection::SignalClientConnectionClosed, this, &ProtoServer::signalClientConnectionClosedHandler);
				connect(client, &ProtoNanoClientConnection::SignalClearGlobalInput, GlobalSignals::getInstance(), &GlobalSignals::SignalClearGlobalInput);
				connect(client, &ProtoNanoClientConnection::SignalImportFromProto, this, &ProtoServer::SignalImportFromProto);
				connect(client, &ProtoNanoClientConnection::SignalSetGlobalColor, GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalColor);
				_openConnections.append(client);
			}
			else
				socket->close();
		}
	}
}

void ProtoServer::signalClientConnectionClosedHandler(ProtoNanoClientConnection* client)
{
	if (client != nullptr)
	{
		client->deleteLater();
		_openConnections.removeAll(client);
	}
}

void ProtoServer::startServer()
{
	if (!_server->isListening())
	{
		if (!_server->listen(QHostAddress::Any, _port))
		{
			Error(_log, "Failed to bind port {:d}", _port);
		}
		else
		{
			Info(_log, "Started on port {:d}", _port);
		}
	}
}

void ProtoServer::stopServer()
{
	if (_server->isListening())
	{
		QVectorIterator<ProtoNanoClientConnection*> i(_openConnections);
		while (i.hasNext())
		{
			const auto& client = i.next();
			client->forceClose();
		}
		_server->close();
		Info(_log, "Stopped");
	}
}
