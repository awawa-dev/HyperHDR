#include <ProtoNanoClientConnection.h>
#include <ProtoServer.h>

// util
#include <utils/NetOrigin.h>
#include <utils/GlobalSignals.h>

// qt
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

ProtoServer::ProtoServer(const QJsonDocument& config, QObject* parent)
	: QObject(parent)
	, _server(new QTcpServer(this))
	, _log(Logger::getInstance("PROTOSERVER"))
	, _timeout(5000)
	, _config(config)
{

}

ProtoServer::~ProtoServer()
{
	stopServer();
	delete _server;
}

void ProtoServer::initServer()
{
	_netOrigin = NetOrigin::getInstance();
	connect(_server, &QTcpServer::newConnection, this, &ProtoServer::newConnection);

	// apply config
	handleSettingsUpdate(settings::type::PROTOSERVER, _config);
}

void ProtoServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::type::PROTOSERVER)
	{
		const QJsonObject& obj = config.object();

		quint16 port = obj["port"].toInt(19445);

		// port check
		if(_server->serverPort() != port)
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
	while(_server->hasPendingConnections())
	{
		if(QTcpSocket * socket = _server->nextPendingConnection())
		{
			if(_netOrigin->accessAllowed(socket->peerAddress(), socket->localAddress()))
			{
				Debug(_log, "New connection from %s", QSTRING_CSTR(socket->peerAddress().toString()));
				ProtoNanoClientConnection* client = new ProtoNanoClientConnection(socket, _timeout, this);
				// internal
				connect(client, &ProtoNanoClientConnection::clientDisconnected, this, &ProtoServer::clientDisconnected);
				connect(client, &ProtoNanoClientConnection::registerGlobalInput, GlobalSignals::getInstance(), &GlobalSignals::registerGlobalInput);
				connect(client, &ProtoNanoClientConnection::clearGlobalInput, GlobalSignals::getInstance(), &GlobalSignals::clearGlobalInput);
				connect(client, &ProtoNanoClientConnection::setGlobalInputImage, GlobalSignals::getInstance(), &GlobalSignals::setGlobalImage);
				connect(client, &ProtoNanoClientConnection::setGlobalInputColor, GlobalSignals::getInstance(), &GlobalSignals::setGlobalColor);
				connect(GlobalSignals::getInstance(), &GlobalSignals::globalRegRequired, client, &ProtoNanoClientConnection::registationRequired);
				_openConnections.append(client);
			}
			else
				socket->close();
		}
	}
}

void ProtoServer::clientDisconnected()
{
	ProtoNanoClientConnection* client = qobject_cast<ProtoNanoClientConnection*>(sender());
	client->deleteLater();
	_openConnections.removeAll(client);
}

void ProtoServer::startServer()
{
	if(!_server->isListening())
	{
		if(!_server->listen(QHostAddress::Any, _port))
		{
		Error(_log,"Failed to bind port %d", _port);
		}
		else
		{
		Info(_log,"Started on port %d", _port);
		}
	}
}

void ProtoServer::stopServer()
{
	if(_server->isListening())
	{
		// close client connections
		for(const auto& client : _openConnections)
		{
			client->forceClose();
		}
		_server->close();
		Info(_log, "Stopped");
	}
}
