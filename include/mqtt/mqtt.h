#pragma once

#ifndef PCH_ENABLED
	#include <QSet>
	#include <QJsonDocument>
	#include <QTimer>
#endif

#include <utils/Logger.h>
#include <utils/Components.h>
#include <utils/settings.h>

#include <qmqtt.h>

class mqtt : public QObject
{
	Q_OBJECT

public:
	mqtt(const QJsonDocument& mqttConfig);
	~mqtt();

public slots:
	void begin();

	void start(QString host, int port, QString username, QString password, bool is_ssl, bool ignore_ssl_errors, QString customTopic);
	void stop();

	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

private slots:
	void connected();
	void error(const QMQTT::ClientError error);
	void received(const QMQTT::Message& message);
	void disconnected();

private:
	void executeJson(QString origin, const QJsonDocument& input, QJsonDocument& result);
	void initRetry();

	// HyperHDR MQTT topic & reponse path
	QString			HYPERHDRAPI;
	QString			HYPERHDRAPI_RESPONSE;

	QString		_customTopic;

	bool		_enabled;
	QString		_host;
	int			_port;
	QString		_username;
	QString		_password;
	bool		_is_ssl;
	bool		_ignore_ssl_errors;
	int			_maxRetry;
	int			_currentRetry;
	QTimer*		_retryTimer;
	bool		_initialized;
	QJsonArray	_resultArray;

	Logger*		_log;
	QMQTT::Client*	_clientInstance;	
};
