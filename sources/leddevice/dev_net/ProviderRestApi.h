#ifndef PROVIDERRESTKAPI_H
#define PROVIDERRESTKAPI_H

// Local-HyperHDR includes
#include <utils/Logger.h>

// Qt includes
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QThread>
#include <QJsonDocument>
#include <memory>

class httpResponse;
class networkHelper;
///
/// Wrapper class supporting REST-API calls with JSON requests and responses
///
/// Usage sample:
/// @code
///
/// ProviderRestApi* _restApi = new ProviderRestApi(hostname, port );
///
/// _restApi->setBasePath( QString("/api/%1/").arg(token) );
/// _restApi->setPath( QString("%1/%2").arg( "groups" ).arg( groupId ) );
///
/// httpResponse response = _restApi->get();
/// if ( !response.error() )
///		response.getBody();
///
/// delete _restApi;
///
///@endcode
///
class ProviderRestApi : public QObject
{
	Q_OBJECT

public:

	///
	/// @brief Constructor of the REST-API wrapper
	///
	ProviderRestApi();

	///
	/// @brief Constructor of the REST-API wrapper
	///
	/// @param[in] host
	/// @param[in] port
	///
	ProviderRestApi(const QString& host, int port);

	///
	/// @brief Constructor of the REST-API wrapper
	///
	/// @param[in] host
	/// @param[in] port
	/// @param[in] API base-path
	///
	ProviderRestApi(const QString& host, int port, const QString& basePath);

	~ProviderRestApi();

	void  updateHost(const QString& host, int port);

	///
	/// @brief Get the URL as defined using scheme, host, port, API-basepath, path, query, fragment
	///
	/// @return url
	///
	QUrl getUrl() const;

	///
	/// @brief Set an API's base path (the stable path element before addressing resources)
	///
	/// @param[in] basePath, e.g. "/api/v1/" or "/json"
	///
	void setBasePath(const QString& basePath);

	///
	/// @brief Set an API's path to address resources
	///
	/// @param[in] path, e.g. "/lights/1/state/"
	///
	void setPath(const QString& path);

	///
	/// @brief Append an API's path element to path set before
	///
	/// @param[in] path
	///
	void appendPath(const QString& appendPath);

	///
	/// @brief Set an API's fragment
	///
	/// @param[in] fragment, e.g. "question3"
	///
	void setFragment(const QString& fragment);

	///
	/// @brief Set an API's query string
	///
	/// @param[in] query, e.g. "&A=128&FX=0"
	///
	void setQuery(const QUrlQuery& query);

	///
	/// @brief Execute GET request
	///
	/// @return Response The body of the response in JSON
	///
	httpResponse get();

	///
	/// @brief Execute GET request
	///
	/// @param[in] url GET request for URL
	/// @return Response The body of the response in JSON
	///
	httpResponse get(const QUrl& url);

	///
	/// @brief Execute PUT request
	///
	/// @param[in] body The body of the request in JSON
	/// @return Response The body of the response in JSON
	///
	httpResponse put(const QString& body = "");

	///
	/// @brief Handle responses for REST requests
	///
	/// @param[in] reply Network reply
	/// @return Response The body of the response in JSON
	///
	httpResponse getResponse(QNetworkReply* const& reply, bool timeout);

private:

	///
	/// @brief Execute PUT request
	///
	/// @param[in] URL for PUT request
	/// @param[in] body The body of the request in JSON
	/// @return Response The body of the response in JSON
	///
	httpResponse put(const QUrl& url, const QString& body = "");

	///
	/// @brief Append an API's path element to path given as param
	///
	/// @param[in/out] path to be updated
	/// @param[in] path, element to be appended
	///
	void appendPath(QString& path, const QString& appendPath) const;

	Logger* _log;

	QUrl      _apiUrl;

	QString   _scheme;
	QString   _hostname;
	int       _port;

	QString   _basePath;
	QString   _path;

	QString   _fragment;
	QUrlQuery _query;

	static    std::unique_ptr<networkHelper> _networkWorker;
};

class networkHelper : public QObject
{
	Q_OBJECT

public:
	QNetworkAccessManager* _networkManager;

	networkHelper();

	~networkHelper();

public slots:
	QNetworkReply* get(QNetworkRequest request);

	QNetworkReply* put(QNetworkRequest request, QByteArray body);
};


///
/// Response object for REST-API calls and JSON-responses
///
class httpResponse
{
public:
	httpResponse() = default;

	bool error() const;
	void setError(const bool hasError);

	QJsonDocument getBody() const;
	void setBody(const QJsonDocument& body);

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

	int _httpStatusCode = 0;
	QNetworkReply::NetworkError _networkReplyError = QNetworkReply::NoError;
};


#endif // PROVIDERRESTKAPI_H
