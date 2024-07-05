/* ProviderRestApi.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

// Local-HyperHDR includes
#include <led-drivers/net/ProviderRestApi.h>
#include <led-drivers/LedDevice.h>

// Qt includes
#include <QNetworkReply>
#include <QByteArray>
#include <QThread>
#include <QTimer>
#include <QCoreApplication>
#include <QElapsedTimer>

//std includes
#include <iostream>

const int TIMEOUT = (500);

ProviderRestApi::ProviderRestApi(const QString& host, int port, const QString& basePath)
	:_log(Logger::getInstance("LEDDEVICE"))
	, _scheme((port == 443) ? "https" : "http")
	, _hostname(host)
	, _port(port)
{
	qRegisterMetaType<QNetworkRequest>();
	qRegisterMetaType<QUrl>();
	qRegisterMetaType<QByteArray>();

	_apiUrl.setScheme(_scheme);
	_apiUrl.setHost(host);
	_apiUrl.setPort(port);
	_basePath = basePath;

	_workerThread = NetworkHelper::threadFactory();
}

ProviderRestApi::ProviderRestApi(const QString& host, int port)
	: ProviderRestApi(host, port, "") {}

ProviderRestApi::ProviderRestApi()
	: ProviderRestApi("", -1) {}

ProviderRestApi::~ProviderRestApi()
{

}

void  ProviderRestApi::updateHost(const QString& host, int port)
{
	_hostname = host;
	_port = port;
}

void ProviderRestApi::setBasePath(const QString& basePath)
{
	_basePath = basePath;
}

void ProviderRestApi::setPath(const QString& path)
{
	_path = path;
}

void ProviderRestApi::appendPath(QString& path, const QString& appendPath) const
{
	auto list = path.split('/', Qt::SkipEmptyParts);
	list.append(appendPath.split('/', Qt::SkipEmptyParts));
	path = (list.size()) ? "/" + list.join('/') : "";
}

void ProviderRestApi::addHeader(const QString &key, const QString &value)
{
	_headers.insert(key, value);
}

void ProviderRestApi::setFragment(const QString& fragment)
{
	_fragment = fragment;
}

void ProviderRestApi::setQuery(const QUrlQuery& query)
{
	_query = query;
}

QUrl ProviderRestApi::getUrl() const
{
	QUrl url = _apiUrl;

	QString fullPath = _basePath;
	appendPath(fullPath, _path);

	url.setPath(fullPath);
	url.setFragment(_fragment);
	url.setQuery(_query);
	return url;
}

httpResponse ProviderRestApi::put(const QUrl& url, const QString& body)
{
	return executeOperation(QNetworkAccessManager::PutOperation, url, body);
}

httpResponse ProviderRestApi::post(const QUrl& url, const QString& body)
{
	return executeOperation(QNetworkAccessManager::PostOperation, url, body);
}

httpResponse ProviderRestApi::get(const QUrl& url)
{
	return executeOperation(QNetworkAccessManager::GetOperation, url);
}

httpResponse ProviderRestApi::put(const QString& body)
{
	return put(getUrl(), body);
}

httpResponse ProviderRestApi::post(const QString& body)
{
	return post(getUrl(), body);
}


httpResponse ProviderRestApi::get()
{
	return get(getUrl());
}

const QMap<QString, QString>& ProviderRestApi::getHeaders() {
	return _headers;
}

httpResponse ProviderRestApi::executeOperation(QNetworkAccessManager::Operation op, const QUrl& url, const QString& body)
{
	httpResponse response;
	qint64 begin = InternalClock::nowPrecise();
	QString opCode = (op == QNetworkAccessManager::PutOperation) ? "PUT" :
						 (op == QNetworkAccessManager::PostOperation) ? "POST" :
						 (op == QNetworkAccessManager::GetOperation) ? "GET" : "";

	if (opCode.length() == 0)
	{
		Error(_log, "Unsupported opertion code");
		return response;
	}

	Debug(_log, "%s begin: [%s] [%s]", QSTRING_CSTR(opCode), QSTRING_CSTR(url.toString()), QSTRING_CSTR(body));

	// Perform request
	NetworkHelper* networkHelper(new NetworkHelper());

	networkHelper->moveToThread(_workerThread.get());

	_resultLocker.lock();

	BLOCK_CALL_5(networkHelper, executeOperation, ProviderRestApi*, this, QNetworkAccessManager::Operation, op, QUrl, url, QString, body, httpResponse*, &response);

	if (_resultLocker.try_lock_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(TIMEOUT)))
	{
		_resultLocker.unlock();
	}
	else
	{
		QUEUE_CALL_0(networkHelper, abortOperation);
		_resultLocker.lock();
		_resultLocker.unlock();
	}

	qint64 timeTotal = InternalClock::nowPrecise() - begin;

	Debug(_log, "%s end (%lld ms): [%s] [%s]", QSTRING_CSTR(opCode), timeTotal, QSTRING_CSTR(url.toString()), QSTRING_CSTR(body));	

	if (response.error())
		Error(_log, "Reply error. Reason: %s", QSTRING_CSTR(response.getErrorReason()));
	else
		Debug(_log, "Reply OK [%d]", response.getHttpStatusCode());

	networkHelper->deleteLater();

	return response;
}

void ProviderRestApi::releaseResultLock()
{
	_resultLocker.unlock();
}

std::shared_ptr<QThread> NetworkHelper::threadFactory()
{
	static QMutex locker;
	static std::weak_ptr<QThread> persist;
	QMutexLocker lockThis(&locker);

	auto result = persist.lock();
	if (!result)
	{
		result = std::shared_ptr<QThread>(new QThread(), [](QThread* thread) {
			hyperhdr::THREAD_REMOVER(QString("NetworkHelperThread"), thread, nullptr);
		});
		result->start();
		persist = result;
	}

	return result;
}

void NetworkHelper::abortOperation()
{
	if (_networkReply != nullptr)
	{
		_timeout = true;		
		_networkReply->abort();
	}
}

NetworkHelper::NetworkHelper() :
	_networkManager(nullptr),
	_networkReply(nullptr),
	_timeout(false)
{
}

NetworkHelper::~NetworkHelper()
{
	delete _networkReply;
	delete _networkManager;
}

void NetworkHelper::executeOperation(ProviderRestApi* parent, QNetworkAccessManager::Operation op, QUrl url, QString body, httpResponse* response)
{
	QNetworkRequest request(url);

	QMapIterator<QString, QString> i = parent->getHeaders();
	while (i.hasNext())
	{
		i.next();
		request.setRawHeader(i.key().toUtf8(), i.value().toUtf8());
	}

	_networkManager = new QNetworkAccessManager();

	connect(_networkManager, &QNetworkAccessManager::sslErrors, this, [](QNetworkReply* reply, const QList<QSslError>& errors) {
		reply->ignoreSslErrors(errors);
	});

	_networkReply = (op == QNetworkAccessManager::PutOperation) ? _networkManager->put(request, body.toUtf8()) :
		(op == QNetworkAccessManager::PostOperation) ? _networkManager->post(request, body.toUtf8()) : _networkManager->get(request);

	connect(_networkReply, &QNetworkReply::finished, this, [=]() {getResponse(response);  parent->releaseResultLock(); }, Qt::DirectConnection);
}

void NetworkHelper::getResponse(httpResponse* response)
{
	if (_networkReply != nullptr)
	{
		int httpStatusCode = (_timeout) ? 408 : _networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		response->setHttpStatusCode(httpStatusCode);

		QMap<QString, QString> headers;
		for (const auto& item: _networkReply->rawHeaderPairs())
		{
			headers[item.first] = item.second;
		}
		response->setHeaders(headers);

		if (_timeout)
			response->setNetworkReplyError(QNetworkReply::TimeoutError);
		else
			response->setNetworkReplyError(_networkReply->error());

		if (_networkReply->error() == QNetworkReply::NoError)
		{
			if (httpStatusCode != 204) {
				QByteArray replyData = _networkReply->readAll();

				if (!replyData.isEmpty())
				{
					QJsonParseError error;
					QJsonDocument jsonDoc = QJsonDocument::fromJson(replyData, &error);

					if (error.error != QJsonParseError::NoError)
					{
						response->setError(true);
						response->setErrorReason(error.errorString());
					}
					else
					{
						response->setBody(jsonDoc);
					}
				}
			}
		}
		else
		{
			if (httpStatusCode > 0)
			{
				QString httpReason = _networkReply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
				QString advise;
				switch (httpStatusCode) {
					case 400:
						advise = "Check Request Body";
						break;
					case 401:
						advise = "Check Authentication Token (API Key)";
						break;
					case 404:
						advise = "Check Resource given";
						break;
					case 408:
						advise = "Check the status of your network and whether the destination IP address is correct";
						httpReason = "Timeout";
						break;
					default:
						break;
				}
				response->setErrorReason(QString("[%3 %4] - %5").arg(QString::number(httpStatusCode), httpReason, advise));
			}
			else
			{
				response->setErrorReason(_networkReply->errorString());
			}
			response->setError(true);
		}

		// clean up
		_networkReply->deleteLater();
		_networkReply = nullptr;
		_networkManager->deleteLater();
		_networkManager = nullptr;
	}
}

bool httpResponse::error() const
{
	return _hasError;
}

void httpResponse::setError(const bool hasError)
{
	_hasError = hasError;
}

QJsonDocument httpResponse::getBody() const
{
	return _responseBody;
}

void httpResponse::setBody(const QJsonDocument& body)
{
	_responseBody = body;
}

QString httpResponse::getErrorReason() const
{
	return _errorReason;
}

void httpResponse::setErrorReason(const QString& errorReason)
{
	_errorReason = errorReason;
}

int httpResponse::getHttpStatusCode() const
{
	return _httpStatusCode;
}

void httpResponse::setHttpStatusCode(int httpStatusCode)
{
	_httpStatusCode = httpStatusCode;
}

QNetworkReply::NetworkError httpResponse::getNetworkReplyError() const
{
	return _networkReplyError;
}

void httpResponse::setNetworkReplyError(const QNetworkReply::NetworkError networkReplyError)
{
	_hasError = (networkReplyError != QNetworkReply::NetworkError::NoError);
	_networkReplyError = networkReplyError;
}

QMap<QString, QString> httpResponse::getHeaders() const
{
	return _headers;
}

void httpResponse::setHeaders(const QMap<QString, QString> &h)
{
	_headers = h;
}
