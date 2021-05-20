// system includes
#include <stdexcept>

// project includes
#include <boblightserver/BoblightServer.h>
#include "BoblightClientConnection.h"

#include <hyperhdrbase/HyperHdrInstance.h>

// qt incl
#include <QTcpServer>

// netUtil
#include <utils/NetUtils.h>

using namespace hyperhdr;

BoblightServer::BoblightServer(HyperHdrInstance* hyperhdr,const QJsonDocument& config)
	: QObject()
	, _hyperhdr(hyperhdr)
	, _server(new QTcpServer(this))
	, _openConnections()
	, _priority(0)
	, _log(Logger::getInstance("BOBLIGHT"))
	, _port(0)
{
	Info(_log, "Instance created");

	// listen for component change
	connect(_hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &BoblightServer::compStateChangeRequest);
	// listen new connection signal from server
	connect(_server, &QTcpServer::newConnection, this, &BoblightServer::newConnection);

	// init
	handleSettingsUpdate(settings::type::BOBLSERVER, config);
}

BoblightServer::~BoblightServer()
{
	stop();
}

void BoblightServer::start()
{
	if ( _server->isListening() )
		return;

	if (NetUtils::portAvailable(_port, _log))
		_server->listen(QHostAddress::Any, _port);

	Info(_log, "Started on port %d", _port);

	_hyperhdr->setNewComponentState(COMP_BOBLIGHTSERVER, _server->isListening());
}

void BoblightServer::stop()
{
	if ( ! _server->isListening() )
		return;

	qDeleteAll(_openConnections);

	_server->close();

	Info(_log, "Stopped");
	_hyperhdr->setNewComponentState(COMP_BOBLIGHTSERVER, _server->isListening());
}

bool BoblightServer::active() const
{
	return _server->isListening();
}

void BoblightServer::compStateChangeRequest(hyperhdr::Components component, bool enable)
{
	if (component == COMP_BOBLIGHTSERVER)
	{
		if (_server->isListening() != enable)
		{
			if (enable) start();
			else        stop();
		}
	}
}

uint16_t BoblightServer::getPort() const
{
	return _server->serverPort();
}

void BoblightServer::newConnection()
{
	QTcpSocket * socket = _server->nextPendingConnection();

	if (socket != nullptr)
	{
		Info(_log, "new connection");
		_hyperhdr->registerInput(_priority, hyperhdr::COMP_BOBLIGHTSERVER, QString("Boblight@%1").arg(socket->peerAddress().toString()));
		BoblightClientConnection * connection = new BoblightClientConnection(_hyperhdr, socket, _priority);
		_openConnections.insert(connection);

		// register slot for cleaning up after the connection closed
		connect(connection, &BoblightClientConnection::connectionClosed, this, &BoblightServer::closedConnection);
	}
}

void BoblightServer::closedConnection(BoblightClientConnection *connection)
{
	Debug(_log, "connection closed");
	_openConnections.remove(connection);

	// schedule to delete the connection object
	connection->deleteLater();
}

void BoblightServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::type::BOBLSERVER)
	{
		QJsonObject obj = config.object();
		_port = obj["port"].toInt();
		_priority = obj["priority"].toInt();
		stop();
		if(obj["enable"].toBool())
			start();
	}
}
