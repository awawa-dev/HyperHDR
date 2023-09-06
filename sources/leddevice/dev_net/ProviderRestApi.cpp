/* ProviderRestApi.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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
#include "ProviderRestApi.h"

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

std::unique_ptr<networkHelper> ProviderRestApi::_networkWorker(nullptr);

ProviderRestApi::ProviderRestApi(const QString& host, int port, const QString& basePath)
	:_log(Logger::getInstance("LEDDEVICE"))
	, _scheme((port == 443) ? "https" : "http")
	, _hostname(host)
	, _port(port)
{
	if (_networkWorker == nullptr)
	{
		_networkWorker = std::unique_ptr<networkHelper>(new networkHelper());		
	}

	qRegisterMetaType<QNetworkRequest>();
	qRegisterMetaType<QByteArray>();

	_apiUrl.setScheme(_scheme);
	_apiUrl.setHost(host);
	_apiUrl.setPort(port);
	_basePath = basePath;
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
	_basePath.clear();
	appendPath(_basePath, basePath);
}

void ProviderRestApi::setPath(const QString& path)
{
	_path.clear();
	appendPath(_path, path);
}

void ProviderRestApi::appendPath(const QString& path)
{
	appendPath(_path, path);
}

void ProviderRestApi::appendPath(QString& path, const QString& appendPath) const
{	
	path = QUrl(path).resolved(appendPath).toString();	
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

bool ProviderRestApi::waitForResult(QNetworkReply* networkReply)
{
	bool networkTimeout = false;

	if (!networkReply->isFinished() && networkReply->error() == QNetworkReply::NoError)
	{
		if (!_resultLocker.try_lock_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(TIMEOUT)))
		{
			if (!networkReply->isFinished())
			{
				networkTimeout = true;
				disconnect(networkReply, &QNetworkReply::finished, nullptr, nullptr);
				QTimer::singleShot(0, networkReply, &QNetworkReply::abort);
			}
		}
		else
			_resultLocker.unlock();
	}
	
	return networkTimeout;
}

httpResponse ProviderRestApi::executeOperation(QNetworkAccessManager::Operation op, const QUrl& url, const QString& body)
{
	qint64 begin = InternalClock::nowPrecise();

	QString opCode = (op == QNetworkAccessManager::PutOperation) ? "PUT" :
						 (op == QNetworkAccessManager::PostOperation) ? "POST" :
						 (op == QNetworkAccessManager::GetOperation) ? "GET" : "";

	if (opCode.length() == 0)
	{
		Error(_log, "Unsupported opertion code");
		return httpResponse();
	}

	Debug(_log, "%s begin: [%s] [%s]", QSTRING_CSTR(opCode), QSTRING_CSTR(url.toString()), QSTRING_CSTR(body));


	// Perform request
	QNetworkRequest request(url);
	QNetworkReply* networkReply = nullptr;

	request.setOriginatingObject(this);
	QMapIterator<QString, QString> i(_headers);
	while (i.hasNext())
	{
		i.next();
		request.setRawHeader(i.key().toUtf8(), i.value().toUtf8());
	}

	SAFE_CALL_3_RET(_networkWorker.get(), executeOperation,
		QNetworkReply*, networkReply, QNetworkAccessManager::Operation, op, QNetworkRequest, request, QByteArray, body.toUtf8());


	bool networkTimeout = waitForResult(networkReply);
	long long timeTotal = InternalClock::nowPrecise() - begin;

	Debug(_log, "%s end (%lld ms): [%s] [%s]", QSTRING_CSTR(opCode), timeTotal, QSTRING_CSTR(url.toString()), QSTRING_CSTR(body));

	httpResponse response = (networkReply->operation() == op) ? getResponse(networkReply, networkTimeout) : httpResponse();

	networkReply->deleteLater();

	return response;
}


void ProviderRestApi::aquireResultLock()
{	
	_resultLocker.tryLock();
}

void ProviderRestApi::releaseResultLock()
{
	_resultLocker.unlock();
}

httpResponse ProviderRestApi::getResponse(QNetworkReply* const& reply, bool timeout)
{
	httpResponse response;

	int httpStatusCode = (timeout) ? 408 : reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	response.setHttpStatusCode(httpStatusCode);
	
	QMap<QString, QString> headers;
	// We sometimes need headers in the response to get the hue application id for instance
	for (const auto &item: reply->rawHeaderPairs())
	{
		headers[item.first] = item.second;
	};
	response.setHeaders(headers);
	
	if (timeout)
		response.setNetworkReplyError(QNetworkReply::TimeoutError);
	else
		response.setNetworkReplyError(reply->error());

	if (reply->error() == QNetworkReply::NoError)
	{
		if (httpStatusCode != 204) {
			QByteArray replyData = reply->readAll();

			if (!replyData.isEmpty())
			{
				QJsonParseError error;
				QJsonDocument jsonDoc = QJsonDocument::fromJson(replyData, &error);

				if (error.error != QJsonParseError::NoError)
				{
					//Received not valid JSON response
					//std::cout << "Response: [" << replyData.toStdString() << "]" << std::endl;
					response.setError(true);
					response.setErrorReason(error.errorString());
				}
				else
				{
					//std::cout << "Response: [" << QString (jsonDoc.toJson(QJsonDocument::Compact)).toStdString() << "]" << std::endl;
					response.setBody(jsonDoc);
				}
			}
			else
			{	// Create valid body which is empty
				response.setBody(QJsonDocument());
			}
		}
	}
	else
	{
		QString errorReason;
		if (httpStatusCode > 0) {
			QString httpReason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
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
				advise = "Check if target IP is valid and your network status";
				httpReason = "Timeout";
				break;
			default:
				break;
			}
			errorReason = QString("[%3 %4] - %5").arg(QString::number(httpStatusCode), httpReason, advise);
		}
		else {
			errorReason = reply->errorString();
		}

		response.setError(true);
		response.setErrorReason(errorReason);

		// Create valid body which is empty
		response.setBody(QJsonDocument());
	}

	if (response.error())
		Error(_log, "Reply.httpStatusCode %s", QSTRING_CSTR(response.getErrorReason()));
	else
		Debug(_log, "Reply.httpStatusCode [%d]", httpStatusCode);

	return response;
}

networkHelper::networkHelper()
{
	QThread* parent = new QThread();
	parent->setObjectName("RestApiThread");

	_networkManager = new QNetworkAccessManager();
	_networkManager->moveToThread(parent);
	this->moveToThread(parent);

	parent->start();
}

networkHelper::~networkHelper()
{	
	// get current thread
	QThread* oldThread = _networkManager->thread();
	disconnect(oldThread, nullptr, nullptr, nullptr);
	oldThread->quit();
	oldThread->wait();
	delete oldThread;

	disconnect(_networkManager, nullptr, nullptr, nullptr);
	delete _networkManager;
	_networkManager = nullptr;
}

QNetworkReply* networkHelper::executeOperation(QNetworkAccessManager::Operation op, QNetworkRequest request, QByteArray body)
{
	ProviderRestApi* parent = static_cast<ProviderRestApi*>(request.originatingObject());
	parent->aquireResultLock();

	QSslConfiguration conf = request.sslConfiguration();
	conf.setPeerVerifyMode(QSslSocket::VerifyNone);
	request.setSslConfiguration(conf);

	auto ret = (op == QNetworkAccessManager::PutOperation) ? _networkManager->put(request, body):
				(op == QNetworkAccessManager::PostOperation) ? _networkManager->post(request, body) : _networkManager->get(request);

	connect(ret, &QNetworkReply::finished, this, [=](){parent->releaseResultLock();});
	return ret;
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
