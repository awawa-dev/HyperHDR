#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
#endif

#include <utils/Logger.h>

class QtHttpServer;
class QtHttpRequest;
class QtHttpClientWrapper;
class HyperAPI;
class HyperHdrManager;

class WebJsonRpc : public QObject {
	Q_OBJECT

public:
	WebJsonRpc(QtHttpRequest* request, QtHttpServer* server, bool localConnection, QtHttpClientWrapper* parent);

	void handleMessage(QtHttpRequest* request, QString query = "");

private:
	QtHttpServer* _server;
	QtHttpClientWrapper* _wrapper;
	Logger*		_log;
	HyperAPI*	_hyperAPI;

	bool _stopHandle = false;
	bool _unlocked = false;

private slots:
	void handleCallback(QJsonObject obj);
};
