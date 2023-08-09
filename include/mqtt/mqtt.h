#pragma once

// Qt includes
#include <QSet>
#include <QJsonDocument>

#include <utils/Logger.h>
#include <utils/Components.h>

// settings
#include <utils/settings.h>
#include <qmqtt.h>

class mqtt : public QObject
{
	Q_OBJECT

public:
	mqtt(QObject* _parent);
	~mqtt();

public slots:
	void start(QString host, int port, QString username, QString password, bool is_ssl, bool ignore_ssl_errors, QString customTopic);

	void stop();

	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

private slots:
	void connected();
	void error(const QMQTT::ClientError error);
	void received(const QMQTT::Message& message);

signals:
	void jsonCommander(QString command);

private:

	// HyperHDR MQTT topic & reponse path
	QString			HYPERHDRAPI;
	QString			HYPERHDRAPI_RESPONSE;

	int				_jsonPort;
	Logger*			_log;
	QMQTT::Client*	_clientInstance;
};
