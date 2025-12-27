#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QByteArray>
	#include <QHash>
	#include <QList>
#endif

class QtHttpServer;

class QtHttpReply : public QObject
{
	Q_OBJECT
public:
	explicit QtHttpReply(QtHttpServer* parent);

	enum StatusCode
	{
		Ok = 200,
		SeeOther = 303,
		BadRequest = 400,
		Forbidden = 403,
		NotFound = 404,
		MethodNotAllowed = 405,
		InternalError = 500,
		NotImplemented = 501,
		BadGateway = 502,
		ServiceUnavailable = 503,
	};
	Q_ENUM(StatusCode)

	int               getRawDataSize(void) const;
	bool              useChunked(void) const;
	StatusCode        getStatusCode(void) const;
	QByteArray        getRawData(void) const;
	QList<QByteArray> getHeadersList(void) const;

	QByteArray getHeader(const QByteArray& header) const;

	static const QByteArray getStatusTextForCode(StatusCode statusCode);

public slots:
	void setUseChunked(bool chunked = false);
	void setStatusCode(QtHttpReply::StatusCode statusCode);
	void appendRawData(const QByteArray& data);
	void addHeader(const QByteArray& header, const QByteArray& value);
	void resetRawData(void);

signals:
	void SignalRequestSendHeaders(void);
	void SignalRequestSendData(void);

private:
	bool							m_useChunked;
	StatusCode						m_statusCode;
	QByteArray						m_data;
	QtHttpServer*					m_serverHandle;
	QHash<QByteArray, QByteArray>	m_headersHash;
};
