#include <QTcpSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>

#include "WebJsonRpc.h"
#include "QtHttpReply.h"
#include "QtHttpRequest.h"
#include "QtHttpServer.h"
#include "QtHttpClientWrapper.h"

#include <api/HyperAPI.h>

#define MULTI_REQ "&request="

WebJsonRpc::WebJsonRpc(QtHttpRequest* request, QtHttpServer* server, bool localConnection, QtHttpClientWrapper* parent)
	: QObject(parent)
	, _server(server)
	, _wrapper(parent)
	, _log("HTTPJSONRPC")
{
	const QString client = request->getClientInfo().clientAddress.toString();
	_hyperAPI = new HyperAPI(client, _log, localConnection, this, true);
	connect(_hyperAPI, &HyperAPI::SignalCallbackJsonMessage, this, &WebJsonRpc::handleCallback);
	connect(_hyperAPI, &HyperAPI::SignalPerformClientDisconnection, this, [&]() { _wrapper->closeConnection(); _stopHandle = true; });
	_hyperAPI->initialize();
}


void WebJsonRpc::handleCallback(QJsonObject obj)
{
	// guard against wrong callbacks; TODO: Remove when JSONAPI is more solid
	if (!_unlocked) return;
	_unlocked = false;
	// construct reply with headers timestamp and server name
	QtHttpReply reply(_server);
	QJsonDocument doc(obj);
	reply.addHeader("Content-Type", "application/json");
	reply.appendRawData(doc.toJson());
	_wrapper->sendToClientWithReply(&reply);
}

void WebJsonRpc::handleMessage(QtHttpRequest* request, QString query)
{
	// TODO better solution. If jsonAPI emits forceClose the request is deleted and the following call to this method results in segfault
	if (!_stopHandle)
	{
		int nextQueryIndex = query.indexOf(MULTI_REQ, Qt::CaseInsensitive);

		if (nextQueryIndex < 0)
		{
			const QByteArray& header = request->getHeader("Authorization");
			const QByteArray& data = (query.length() > 0) ? query.toUtf8() : request->getRawData();
			_unlocked = true;
			_hyperAPI->handleMessage(data, header);
		}
		else
		{
			const QByteArray& header = request->getHeader("Authorization");
			_unlocked = true;

			query = QString(MULTI_REQ).append(query);
			do
			{
				query = query.right(query.length() - QString(MULTI_REQ).length());
				nextQueryIndex = query.indexOf(MULTI_REQ, Qt::CaseInsensitive);

				QString leftQuery = (nextQueryIndex >= 0) ? query.left(nextQueryIndex) : query;
				query = query.right(query.length() - leftQuery.length());

				_hyperAPI->handleMessage(leftQuery.toUtf8(), header);

			} while (nextQueryIndex >= 0 && query.length() > QString(MULTI_REQ).length());
		}
	}
}

