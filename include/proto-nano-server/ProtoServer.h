#pragma once

#ifndef PCH_ENABLED
	#include <QVector>
#endif

#include <utils/Logger.h>
#include <utils/settings.h>


class QTcpServer;
class ProtoNanoClientConnection;
class NetOrigin;

class ProtoServer : public QObject
{
	Q_OBJECT

public:
	ProtoServer(std::shared_ptr<NetOrigin> netOrigin, const QJsonDocument& config, QObject* parent = nullptr);
	~ProtoServer() override;

public slots:
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void initServer();

private slots:
	void newConnection();
	void signalClientConnectionClosedHandler(ProtoNanoClientConnection* client);

private:
	void startServer();
	void stopServer();

signals:
	void SignalImportFromProto(int priority, int duration, const Image<ColorRgb>& image, QString clientDescription);

private:
	QTcpServer*	_server;
	std::shared_ptr<NetOrigin> _netOrigin;
	Logger*		_log;
	int			_timeout;
	quint16		_port;

	const QJsonDocument _config;
	QVector<ProtoNanoClientConnection*> _openConnections;
};
