#pragma once

#ifndef PCH_ENABLED
	#include <QList>
	#include <QStringList>
	#include <QHostAddress>
	#include <QJsonObject>
	#include <QJsonArray>
	#include <QJsonDocument>

	#include <vector>
	#include <map>
	#include <cstdint>
	#include <limits>

	#include <utils/ColorRgb.h>
	#include <utils/Image.h>
	#include <utils/Logger.h>
	#include <utils/settings.h>
	#include <utils/Components.h>
#endif

// Forward declaration
class HyperHdrInstance;
class QTcpSocket;
class FlatBufferConnection;

class NetworkForwarder : public QObject
{
	Q_OBJECT

public:
	NetworkForwarder();
	~NetworkForwarder() override;

	void addJsonSlave(const QString& slave, const QJsonObject& obj);
	void addFlatbufferSlave(const QString& slave, const QJsonObject& obj);

signals:
	void SignalForwardImage();

public slots:
	void startedHandler();
	void signalForwardImageHandler();
	void handlerInstanceImageUpdated(const Image<ColorRgb>& ret);

private slots:
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void handleCompStateChangeRequest(hyperhdr::Components component, bool enable);
	void forwardJsonMessage(const QJsonObject& message);
	void sendJsonMessage(const QJsonObject& message, QTcpSocket* socket);

private:
	std::weak_ptr<HyperHdrInstance> _instanceZero;

	Logger* _log;
	QStringList   _jsonSlaves;
	QStringList _flatSlaves;
	bool _forwarderEnabled;

	const int	_priority;

	QList<FlatBufferConnection*> _forwardClients;
	std::atomic<bool> _hasImage;
	Image<ColorRgb>	_image;
};
