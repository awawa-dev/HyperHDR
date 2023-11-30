// system includes
#include <stdexcept>

// project includes
#include "HyperhdrConfig.h"
#include <jsonserver/JsonServer.h>
#include "JsonClientConnection.h"

#include <utils/NetOrigin.h>
#include <api/HyperAPI.h>

// qt includes
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QByteArray>

JsonServer::JsonServer(std::shared_ptr<NetOrigin> netOrigin, const QJsonDocument& config)
	: QObject()
	, _server(new QTcpServer(this))
	, _openConnections()
	, _log(Logger::getInstance("JSONSERVER"))
	, _netOrigin(netOrigin)
	, _port(0)
{
	Debug(_log, "Created new instance");

	// Set trigger for incoming connections
	connect(_server, &QTcpServer::newConnection, this, &JsonServer::newConnection);

	// init
	handleSettingsUpdate(settings::type::JSONSERVER, config);
}

JsonServer::~JsonServer()
{
	Debug(_log, "The instance is deleted");
	qDeleteAll(_openConnections);
}

void JsonServer::start()
{
	if (_server->isListening())
		return;

	if (!_server->listen(QHostAddress::Any, _port))
	{
		Error(_log, "Could not bind to port '%d', please use an available port", _port);
		return;
	}

	Info(_log, "Started on port %d", _port);
}

void JsonServer::stop()
{
	if (!_server->isListening())
		return;

	_server->close();
	Info(_log, "Stopped");
}

void JsonServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::JSONSERVER)
	{
		QJsonObject obj = config.object();
		if (_port != obj["port"].toInt())
		{
			_port = obj["port"].toInt();
			stop();
			start();
		}
	}
}

uint16_t JsonServer::getPort() const
{
	return _port;
}

void JsonServer::newConnection()
{
	while (_server->hasPendingConnections())
	{
		if (QTcpSocket* socket = _server->nextPendingConnection())
		{
			if (_netOrigin->accessAllowed(socket->peerAddress(), socket->localAddress()))
			{
				Debug(_log, "New connection from: %s ", socket->localAddress().toString().toStdString().c_str());
				JsonClientConnection* connection = new JsonClientConnection(socket, _netOrigin->isLocalAddress(socket->peerAddress(), socket->localAddress()));
				_openConnections.insert(connection);

				// register slot for cleaning up after the connection closed
				connect(connection, &JsonClientConnection::SignalClientConnectionClosed, this, &JsonServer::signalClientConnectionClosedHandler);
			}
			else
				socket->close();
		}
	}
}

void JsonServer::signalClientConnectionClosedHandler(JsonClientConnection* client)
{
	if (client != nullptr)
	{
		Debug(_log, "Connection closed");
		_openConnections.remove(client);
		client->deleteLater();
	}
}
