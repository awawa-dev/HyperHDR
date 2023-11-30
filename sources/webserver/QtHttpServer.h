#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <QHash>
	#include <QHostAddress>
#endif

#include <QSslKey>
#include <QTcpServer>
#include <QSslCertificate>
#include <QSslSocket>

class QTcpSocket;
class QtHttpRequest;
class QtHttpReply;
class QtHttpClientWrapper;
class NetOrigin;
class HyperHdrManager;

class QtHttpServerWrapper : public QTcpServer
{
	Q_OBJECT

public:
	explicit QtHttpServerWrapper(QObject* parent = Q_NULLPTR);
	~QtHttpServerWrapper() override;

	void setUseSecure(const bool ssl = true);

protected:
	void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;

private:
	bool m_useSsl;
};

class QtHttpServer : public QObject
{
	Q_OBJECT

public:
	explicit QtHttpServer(std::shared_ptr<NetOrigin> netOrigin, QObject* parent = Q_NULLPTR);

	static const QString& HTTP_VERSION;

	typedef void (QSslSocket::* SslErrorSignal) (const QList<QSslError>&);

	const QString& getServerName(void) const;

	quint16 getServerPort(void) const;
	QString getErrorString(void) const;
	bool    isListening();

public slots:
	void start(quint16 port = 0);
	void stop(void);
	void setUseSecure(const bool ssl = true);
	void setServerName(const QString& serverName);
	void setPrivateKey(const QSslKey& key);
	void setCertificates(const QList<QSslCertificate>& certs);
	QSslKey getPrivateKey();
	QList<QSslCertificate> getCertificates();

signals:
	void started(quint16 port);
	void stopped(void);
	void error(const QString& msg);
	void clientConnected(const QString& guid);
	void clientDisconnected(const QString& guid);
	void requestNeedsReply(QtHttpRequest* request, QtHttpReply* reply);

private slots:
	void onClientConnected(void);
	void onClientDisconnected(void);
	void onClientSslEncrypted(void);
	void onClientSslPeerVerifyError(const QSslError& err);
	void onClientSslErrors(const QList<QSslError>& errors);
	void onClientSslModeChanged(QSslSocket::SslMode mode);

private:
	bool										m_useSsl;
	QSslKey										m_sslKey;
	QList<QSslCertificate>						m_sslCerts;
	QString										m_serverName;
	std::shared_ptr<NetOrigin>					m_netOrigin;
	QtHttpServerWrapper*						m_sockServer;
	QHash<QTcpSocket*, QtHttpClientWrapper*>	m_socksClientsHash;
};
