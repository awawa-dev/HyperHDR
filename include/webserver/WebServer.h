#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <QJsonDocument>
#endif

#include <utils/Logger.h>
#include <utils/settings.h>

class BonjourServiceRegister;
class FileServer;
class QtHttpServer;
class HyperHdrManager;
class NetOrigin;
class QtHttpRequest;
class QtHttpReply;

class WebServer : public QObject
{
	Q_OBJECT

public:
	WebServer(std::shared_ptr<NetOrigin> netOrigin, const QJsonDocument& config, bool useSsl, QObject* parent = nullptr);
	~WebServer() override;

	void start();
	void stop();

	static bool	portAvailable(quint16& port, Logger* log);

signals:
	void stateChange(bool newState);
	void portChanged(quint16 port);

public slots:
	void initServer();
	void onServerStopped();
	void onServerStarted(quint16 port);
	void onServerError(QString msg);
	void handleSettingsUpdate(settings::type type, QJsonDocument config);
	void setSsdpXmlDesc(const QString& desc);
	void onInitRequestNeedsReply(QtHttpRequest* request, QtHttpReply* reply);

	quint16 getPort() const { return _port; }

private:
	quint16              _port;
	QJsonDocument        _config;
	bool				 _useSsl;
	Logger*              _log;
	QString              _baseUrl;
	
	QtHttpServer*        _server;
	std::shared_ptr<NetOrigin> _netOrigin;

	const QString        WEBSERVER_DEFAULT_PATH = ":/www";
	const QString        WEBSERVER_DEFAULT_CRT_PATH = ":/hyperhdrcrt.pem";
	const QString        WEBSERVER_DEFAULT_KEY_PATH = ":/hyperhdrkey.pem";
	quint16              WEBSERVER_DEFAULT_PORT = 8090;

	BonjourServiceRegister* _serviceRegister = nullptr;
	std::shared_ptr<FileServer> _staticFileServing = nullptr;

	static std::weak_ptr<FileServer> globalStaticFileServing;
};
