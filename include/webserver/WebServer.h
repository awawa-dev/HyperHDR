#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QObject>
#include <QString>
#include <QJsonDocument>

// hyperhdr / utils
#include <utils/Logger.h>

// settings
#include <utils/settings.h>

class BonjourServiceRegister;
class StaticFileServing;
class QtHttpServer;

class WebServer : public QObject
{
	Q_OBJECT

public:
	WebServer(const QJsonDocument& config, bool useSsl, QObject* parent = nullptr);

	~WebServer() override;

	void start();
	void stop();

	static bool			portAvailable(quint16& port, Logger* log);

signals:
	///
	/// @emits whenever server is started or stopped (to sync with SSDPHandler)
	/// @param newState   True when started, false when stopped
	///
	void stateChange(bool newState);

	///
	/// @brief Emits whenever the port changes (doesn't compare prev <> now)
	///
	void portChanged(quint16 port);

public slots:
	///
	/// @brief Init server after thread start
	///
	void initServer();

	void onServerStopped();
	void onServerStarted(quint16 port);
	void onServerError(QString msg);

	///
	/// @brief Handle settings update from HyperHDR Settingsmanager emit or this constructor
	/// @param type   settingyType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

	///
	/// @brief Set a new description, if empty the description is NotFound for clients
	///
	void setSSDPDescription(const QString& desc);

	/// check if server has been inited
	bool isInited() const { return _inited; }

	quint16 getPort() const { return _port; }

private:	
	quint16              _port;
	QJsonDocument        _config;
	bool				 _useSsl;
	Logger*              _log;
	QString              _baseUrl;
	StaticFileServing*   _staticFileServing;
	QtHttpServer*        _server;
	bool                 _inited = false;

	const QString        WEBSERVER_DEFAULT_PATH = ":/www";
	const QString        WEBSERVER_DEFAULT_CRT_PATH = ":/hyperhdrcrt.pem";
	const QString        WEBSERVER_DEFAULT_KEY_PATH = ":/hyperhdrkey.pem";
	quint16              WEBSERVER_DEFAULT_PORT = 8090;

	BonjourServiceRegister* _serviceRegister = nullptr;
};

#endif // WEBSERVER_H
