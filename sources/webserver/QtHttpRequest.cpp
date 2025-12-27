#include <QTcpSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>

#include "QtHttpRequest.h"
#include "QtHttpHeader.h"
#include "QtHttpServer.h"

QtHttpRequest::QtHttpRequest(QtHttpClientWrapper* client, QtHttpServer* parent)
	: QObject(parent)
	, m_url(QUrl())
	, m_command(QString())
	, m_data(QByteArray())
	, m_serverHandle(parent)
	, m_clientHandle(client)
	, m_postData(QtHttpPostData())
{
	// set some additional headers
	addHeader(QtHttpHeader::ContentLength, QByteArrayLiteral("0"));
	addHeader(QtHttpHeader::Connection, QByteArrayLiteral("Keep-Alive"));
}

void QtHttpRequest::setClientInfo(const QHostAddress& server, const QHostAddress& client)
{
	m_clientInfo.serverAddress = server;
	m_clientInfo.clientAddress = client;
}

void QtHttpRequest::addHeader(const QByteArray& header, const QByteArray& value)
{
	QByteArray key = header.trimmed();

	if (!key.isEmpty())
	{
		m_headersHash.insert(key, value);
	}
}

int QtHttpRequest::getRawDataSize() const
{
	return m_data.size();
};

QUrl QtHttpRequest::getUrl() const
{
	return m_url;
};

QString QtHttpRequest::getCommand() const
{
	return m_command;
};

QByteArray QtHttpRequest::getRawData() const &
{
	return m_data;
};

QtHttpClientWrapper* QtHttpRequest::getClient() const
{
	return m_clientHandle;
};

QtHttpPostData QtHttpRequest::getPostData() const
{
	return m_postData;
};

QtHttpRequest::ClientInfo QtHttpRequest::getClientInfo() const
{
	return m_clientInfo;
};

QByteArray QtHttpRequest::getHeader(const QByteArray& header) const &
{
	return m_headersHash.value(header, QByteArray());
};

int QtHttpRequest::getHeader(const QByteArray& header, int defValue)
{
	const QByteArray& temp = m_headersHash.value(header, QByteArray());
	bool ok = false;
	int resValue = temp.toInt(&ok);

	if (ok)
		return resValue;
	else
		return defValue;
}


void QtHttpRequest::setUrl(const QUrl& url)
{
	m_url = url;
};

void QtHttpRequest::setCommand(const QString& command)
{
	m_command = command;
};

void QtHttpRequest::appendRawData(const QByteArray& data)
{
	m_data.append(data);
};

void QtHttpRequest::setPostData(const QtHttpPostData& data)
{
	m_postData = data;
};

