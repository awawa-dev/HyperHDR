#include <QTcpSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QUrlQuery>

#include <utils/NetOrigin.h>

#include "QtHttpServer.h"
#include "QtHttpRequest.h"
#include "QtHttpReply.h"
#include "QtHttpClientWrapper.h"

const QString& QtHttpServer::HTTP_VERSION = QStringLiteral("HTTP/1.1");

QtHttpServerWrapper::QtHttpServerWrapper(QObject* parent)
	: QTcpServer(parent)
	, m_useSsl(false)
{

}

QtHttpServerWrapper::~QtHttpServerWrapper(void)
{

}

void QtHttpServerWrapper::setUseSecure(const bool ssl)
{
	m_useSsl = ssl;
}

void QtHttpServerWrapper::incomingConnection(qintptr handle)
{
	QTcpSocket* sock = (m_useSsl ? new QSslSocket(this) : new QTcpSocket(this));
	sock->setSocketDescriptor(handle) ? addPendingConnection(sock) : delete sock;
}

QtHttpServer::QtHttpServer(std::shared_ptr<NetOrigin> netOrigin, QObject* parent)
	: QObject(parent)
	, m_useSsl(false)
	, m_serverName(QStringLiteral("The Qt5 HTTP Server"))
	, m_netOrigin(netOrigin)
{
	m_sockServer = new QtHttpServerWrapper(this);
	connect(m_sockServer, &QtHttpServerWrapper::newConnection, this, &QtHttpServer::onClientConnected);
}

void QtHttpServer::start(quint16 port)
{
	if (!m_sockServer->isListening())
	{
		m_sockServer->listen(QHostAddress::Any, port)
			? emit started(m_sockServer->serverPort())
			: emit error(m_sockServer->errorString());
	}
}

void QtHttpServer::stop(void)
{
	if (m_sockServer->isListening())
	{
		m_sockServer->close();
		// disconnect clients
		const QList<QTcpSocket*> socks = m_socksClientsHash.keys();
		for (auto sock : socks)
		{
			sock->close();
		}

		emit stopped();
	}
}

void QtHttpServer::onClientConnected(void)
{
	while (m_sockServer->hasPendingConnections())
	{
		if (QTcpSocket* sock = m_sockServer->nextPendingConnection())
		{
			if (m_netOrigin->accessAllowed(sock->peerAddress(), sock->localAddress()))
			{
				connect(sock, &QTcpSocket::disconnected, this, &QtHttpServer::onClientDisconnected);

				if (m_useSsl)
				{
					if (QSslSocket* ssl = qobject_cast<QSslSocket*> (sock))
					{
						connect(ssl, SslErrorSignal(&QSslSocket::sslErrors), this, &QtHttpServer::onClientSslErrors);
						connect(ssl, &QSslSocket::encrypted, this, &QtHttpServer::onClientSslEncrypted);
						connect(ssl, &QSslSocket::peerVerifyError, this, &QtHttpServer::onClientSslPeerVerifyError);
						connect(ssl, &QSslSocket::modeChanged, this, &QtHttpServer::onClientSslModeChanged);
						ssl->setLocalCertificateChain(m_sslCerts);
						ssl->setPrivateKey(m_sslKey);
						ssl->setPeerVerifyMode(QSslSocket::AutoVerifyPeer);
						ssl->startServerEncryption();
					}
				}

				QtHttpClientWrapper* wrapper = new QtHttpClientWrapper(
					sock, m_netOrigin->isLocalAddress(sock->peerAddress(), sock->localAddress()), this);
				m_socksClientsHash.insert(sock, wrapper);
				emit clientConnected(wrapper->getGuid());
			}
			else
			{
				sock->close();
			}
		}
	}
}

void QtHttpServer::onClientDisconnected(void)
{
	if (QTcpSocket* sockClient = qobject_cast<QTcpSocket*> (sender()))
	{
		if (QtHttpClientWrapper* wrapper = m_socksClientsHash.value(sockClient, Q_NULLPTR))
		{
			emit clientDisconnected(wrapper->getGuid());
			wrapper->deleteLater();
			m_socksClientsHash.remove(sockClient);
		}
	}
}

void QtHttpServer::setUseSecure(const bool ssl)
{
	m_useSsl = ssl;
	m_sockServer->setUseSecure(m_useSsl);
}

const QString& QtHttpServer::getServerName(void) const
{
	return m_serverName;
};

quint16 QtHttpServer::getServerPort(void) const
{
	return m_sockServer->serverPort();
};

QString QtHttpServer::getErrorString(void) const
{
	return m_sockServer->errorString();
};

bool QtHttpServer::isListening()
{
	return m_sockServer->isListening();
};


void QtHttpServer::setServerName(const QString& serverName)
{
	m_serverName = serverName;
};

void QtHttpServer::setPrivateKey(const QSslKey& key)
{
	m_sslKey = key;
};

void QtHttpServer::setCertificates(const QList<QSslCertificate>& certs)
{
	m_sslCerts = certs;
};

QSslKey QtHttpServer::getPrivateKey()
{
	return m_sslKey;
};

QList<QSslCertificate> QtHttpServer::getCertificates()
{
	return m_sslCerts;
};

void QtHttpServer::onClientSslEncrypted(void)
{
};

void QtHttpServer::onClientSslPeerVerifyError(const QSslError& err)
{
	Q_UNUSED(err)
};

void QtHttpServer::onClientSslErrors(const QList<QSslError>& errors)
{
	Q_UNUSED(errors)
};

void QtHttpServer::onClientSslModeChanged(QSslSocket::SslMode mode)
{
	Q_UNUSED(mode)
};
