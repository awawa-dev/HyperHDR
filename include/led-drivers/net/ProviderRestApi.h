#pragma once

#ifndef PCH_ENABLED
	#include <QMutex>
	#include <QUrlQuery>
	#include <QThread>
	#include <QJsonDocument>
	#include <QNetworkReply>

	#include <memory>	
#endif

#include <utils/Logger.h>


class httpResponse;
class NetworkHelper;
class QNetworkReply;

class ProviderRestApi : public QObject
{
	Q_OBJECT

public:
	ProviderRestApi();
	ProviderRestApi(const QString& host, int port);
	ProviderRestApi(const QString& host, int port, const QString& basePath);
	~ProviderRestApi();

	void  updateHost(const QString& host, int port);
	QUrl getUrl() const;
	void setBasePath(const QString& basePath);
	void setPath(const QString& path);
	void addHeader(const QString &key, const QString &value);
	const QMap<QString, QString>& getHeaders();
	void setFragment(const QString& fragment);
	void setQuery(const QUrlQuery& query);

	httpResponse get();
	httpResponse put(const QString& body = "");
	httpResponse post(const QString& body);
	httpResponse get(const QUrl& url);
	httpResponse put(const QUrl& url, const QString& body = "");
	httpResponse post(const QUrl& url, const QString& body);
	void releaseResultLock();

private:
	void appendPath(QString& path, const QString& appendPath) const;
	httpResponse executeOperation(QNetworkAccessManager::Operation op, const QUrl& url, const QString& body = "");

	Logger*   _log;

	QUrl      _apiUrl;

	QString   _scheme;
	QString   _hostname;
	int       _port;

	QString   _basePath;
	QString   _path;
	QMap<QString, QString> _headers;
	QString   _fragment;
	QUrlQuery _query;

	QMutex    _resultLocker;

	std::shared_ptr<QThread> _workerThread;
};

class NetworkHelper : public QObject
{
	Q_OBJECT

	QNetworkAccessManager* _networkManager;
	QNetworkReply* _networkReply;
	bool _timeout;

	void getResponse(httpResponse* response);

public:
	NetworkHelper();
	~NetworkHelper();

	static std::shared_ptr<QThread> threadFactory();

public slots:
	void executeOperation(ProviderRestApi* parent, QNetworkAccessManager::Operation op, QUrl url, QString body, httpResponse* response);
	void abortOperation();
};


class httpResponse
{
public:
	httpResponse() = default;

	bool error() const;
	void setError(const bool hasError);

	QJsonDocument getBody() const;
	void setBody(const QJsonDocument& body);

     QMap<QString, QString> getHeaders() const;
     void setHeaders( const QMap<QString, QString> &h);

	QString getErrorReason() const;
	void setErrorReason(const QString& errorReason);

	int getHttpStatusCode() const;
	void setHttpStatusCode(int httpStatusCode);

	QNetworkReply::NetworkError getNetworkReplyError() const;
	void setNetworkReplyError(const QNetworkReply::NetworkError networkReplyError);

private:

	QJsonDocument _responseBody;
	bool _hasError = false;
	QString _errorReason;
	QMap<QString, QString> _headers;
	int _httpStatusCode = 0;
	QNetworkReply::NetworkError _networkReplyError = QNetworkReply::NoError;
};
