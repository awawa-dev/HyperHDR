#include <QTcpSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QDateTime>

#include "QtHttpReply.h"
#include "QtHttpHeader.h"
#include "QtHttpServer.h"


QtHttpReply::QtHttpReply(QtHttpServer* parent)
	: QObject(parent)
	, m_useChunked(false)
	, m_statusCode(Ok)
	, m_data(QByteArray())
	, m_serverHandle(parent)
{
	// set some additional headers
	addHeader(QtHttpHeader::Date, QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy hh:mm:ss t").toUtf8());
	addHeader(QtHttpHeader::Server, m_serverHandle->getServerName().toUtf8());
}

void QtHttpReply::addHeader(const QByteArray& header, const QByteArray& value)
{
	QByteArray key = header.trimmed();

	if (!key.isEmpty())
	{
		m_headersHash.insert(key, value);
	}
}

const QByteArray QtHttpReply::getStatusTextForCode(QtHttpReply::StatusCode statusCode)
{
	switch (statusCode)
	{
	case Ok:         return QByteArrayLiteral("OK.");
	case BadRequest: return QByteArrayLiteral("Bad request !");
	case Forbidden:  return QByteArrayLiteral("Forbidden !");
	case NotFound:   return QByteArrayLiteral("Not found !");
	default:         return QByteArrayLiteral("");
	}
}

int QtHttpReply::getRawDataSize(void) const
{
	return m_data.size();
};

bool QtHttpReply::useChunked(void) const
{
	return m_useChunked;
};

QtHttpReply::StatusCode QtHttpReply::getStatusCode(void) const
{
	return m_statusCode;
};

QByteArray QtHttpReply::getRawData(void) const
{
	return m_data;
};

QList<QByteArray> QtHttpReply::getHeadersList(void) const
{
	return m_headersHash.keys();
};

QByteArray QtHttpReply::getHeader(const QByteArray& header) const
{
	return m_headersHash.value(header, QByteArray());
};

void QtHttpReply::setUseChunked(bool chunked)
{
	m_useChunked = chunked;
};

void QtHttpReply::setStatusCode(StatusCode statusCode)
{
	m_statusCode = statusCode;
};

void QtHttpReply::appendRawData(const QByteArray& data)
{
	m_data.append(data);
};

void QtHttpReply::resetRawData(void)
{
	m_data.clear();
};
