#pragma once

#ifndef PCH_ENABLED
	#include <QMap>
	#include <QJsonArray>
	#include <QJsonDocument>
	#include <QObject>
	#include <QTimer>
	#include <QStringList>
#endif

#include <array>
#include <memory>

#include <core_mqtt.h>

#include <utils/Components.h>
#include <utils/Logger.h>
#include <utils/settings.h>

class QAbstractSocket;
struct NetworkContext;

class mqtt : public QObject
{
	Q_OBJECT

public:
	explicit mqtt(const QJsonDocument& mqttConfig);
	~mqtt();

public slots:
	void begin();
	void start(QString host, int port, QString username, QString password, bool is_ssl, bool ignore_ssl_errors, QString customTopic);
	void stop();

	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void handleSignalMqttSubscribe(bool subscribe, QString topic);
	void handleSignalMqttPublish(QString topic, QString payload);
	void handleSignalMqttLastWill(QString id, QStringList pairs);

private:
	static constexpr size_t NetworkBufferSize = 32 * 1024;
	static constexpr size_t QosRecordCount = 16;
	static constexpr uint32_t ConnectTimeoutMs = 2'000;
	static constexpr uint32_t ConnectReceiveWaitMs = 50;
	static constexpr int RetryIntervalMs = 10'000;

	using QosRecords = std::array<MQTTPubAckInfo_t, QosRecordCount>;

	static uint32_t nowMs();
	static int32_t transportSend(NetworkContext_t* context, const void* buffer, size_t size);
	static int32_t transportRecv(NetworkContext_t* context, void* buffer, size_t size);
	static bool eventCallback(MQTTContext_t* context, MQTTPacketInfo_t* packetInfo, MQTTDeserializedInfo_t* deserializedInfo, MQTTSuccessFailReasonCode_t* reasonCode, MQTTPropBuilder_t* sendProps, MQTTPropBuilder_t* getProps);

	void connectMqtt();
	void processLoop();
	void received(const QString& topic, const QString& payload);
	void connected();
	void disconnected();
	void error(MQTTStatus_t status);
	void transportFailure();
	void initRetry();
	void detachSocket();
	void subscribe(const QString& topic, MQTTQoS_t qos);
	void unsubscribe(const QString& topic);
	void publish(const QString& topic, const QString& payload, MQTTQoS_t qos = MQTTQoS0);
	void executeJson(const QString& origin, const QJsonDocument& input, QJsonDocument& result);

private:
	QString HYPERHDRAPI;
	QString HYPERHDRAPI_RESPONSE;
	QString _customTopic;

	bool _enabled = false;
	QString _host;
	int _port = 1883;
	QString _username;
	QString _password;
	bool _is_ssl = false;
	bool _ignore_ssl_errors = true;
	int _maxRetry = 0;
	int _currentRetry = 0;
	bool _initialized = false;
	bool _connected = false;
	bool _connecting = false;
	bool _stopping = false;
	bool _brokerDisconnected = false;
	bool _disableApiAccess = false;
	uint16_t _apiSubscribePacketId = MQTT_PACKET_ID_INVALID;

	QTimer* _processTimer = nullptr;
	QTimer* _retryTimer = nullptr;
	QAbstractSocket* _socket = nullptr;
	std::unique_ptr<NetworkContext> _network;

	TransportInterface_t _transport{};
	MQTTContext_t _mqttContext{};
	MQTTFixedBuffer_t _fixedBuffer{};
	std::array<uint8_t, NetworkBufferSize> _networkBuffer{};
	QosRecords _outgoingQos{};
	QosRecords _incomingQos{};

	QJsonArray _resultArray;
	QMap<QString, QStringList> _lastWill;
	LoggerName _log{ "MQTT" };
};
