// project includes
#include <mqtt/mqtt.h>

#include <QAbstractSocket>
#include <QHostInfo>
#include <QJsonObject>
#include <QSslError>
#include <QSslSocket>
#include <QTcpSocket>

#include <algorithm>
#include <utility>

#include <api/HyperAPI.h>
#include <utils/GlobalSignals.h>
#include <utils/InternalClock.h>

namespace
{
	constexpr const char* TEMPLATE_HYPERHDRAPI = "%1/JsonAPI";
	constexpr const char* TEMPLATE_HYPERHDRAPI_RESPONSE = "%1/JsonAPI/response";

	int keepAliveTimerMs(const MQTTContext_t& context)
	{
		return (context.keepAliveIntervalSec == 0) ? 0 : std::clamp(static_cast<int>(context.keepAliveIntervalSec) * 250, 250, 1000);
	}
}

struct NetworkContext
{
	mqtt* owner = nullptr;
	QAbstractSocket* socket = nullptr;
	bool waitForData = false;
};

mqtt::mqtt(const QJsonDocument& mqttConfig)
{
	_processTimer = new QTimer(this);
	connect(_processTimer, &QTimer::timeout, this, &mqtt::processLoop);

	_retryTimer = new QTimer(this);
	_retryTimer->setSingleShot(true);
	connect(_retryTimer, &QTimer::timeout, this, [this]
		{
			if (!_connected && !_connecting && ++_currentRetry <= _maxRetry) {
				Debug(_log, "Retrying {:d}/{:d}", _currentRetry, _maxRetry);
				start(_host, _port, _username, _password, _is_ssl, _ignore_ssl_errors, _customTopic);
			}
		});

	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalMqttLastWill, this, &mqtt::handleSignalMqttLastWill);
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalMqttSubscribe, this, &mqtt::handleSignalMqttSubscribe);
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalMqttPublish, this, &mqtt::handleSignalMqttPublish);

	handleSettingsUpdate(settings::type::MQTT, mqttConfig);
}

mqtt::~mqtt()
{
	Debug(_log, "Prepare to shutdown");
	stop();
	Debug(_log, "MQTT server is closed");
}

uint32_t mqtt::nowMs()
{
	return static_cast<uint32_t>(InternalClock::now());
}

int32_t mqtt::transportSend(NetworkContext_t* context, const void* buffer, size_t size)
{
	if (size == 0)
		return 0;

	if (!context || !context->socket || context->socket->state() != QAbstractSocket::ConnectedState)
		return -1;

	const qint64 written = context->socket->write(static_cast<const char*>(buffer), static_cast<qint64>(size));
	return written < 0 ? -1 : static_cast<int32_t>(written);
}

int32_t mqtt::transportRecv(NetworkContext_t* context, void* buffer, size_t size)
{
	if (size == 0)
		return 0;
	if (!context || !context->socket || context->socket->state() != QAbstractSocket::ConnectedState)
		return -1;

	auto* socket = context->socket;
	if (socket->bytesAvailable() == 0 && context->waitForData)
		socket->waitForReadyRead(ConnectReceiveWaitMs);

	if (socket->bytesAvailable() == 0)
		return 0;

	const qint64 read = socket->read(static_cast<char*>(buffer), static_cast<qint64>(size));
	return read < 0 ? -1 : static_cast<int32_t>(read);
}

bool mqtt::eventCallback(MQTTContext_t* context, MQTTPacketInfo_t* packetInfo, MQTTDeserializedInfo_t* deserializedInfo, MQTTSuccessFailReasonCode_t*, MQTTPropBuilder_t*, MQTTPropBuilder_t*)
{
	if (!context || !packetInfo || !deserializedInfo)
		return true;

	auto* network = static_cast<NetworkContext_t*>(context->transportInterface.pNetworkContext);
	if (!network || !network->owner)
		return false;

	auto* self = network->owner;
	if ((packetInfo->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH)
	{
		const auto* publish = deserializedInfo->pPublishInfo;
		if (!publish || !publish->pTopicName)
			return true;

		const QString topic = QString::fromUtf8(publish->pTopicName, static_cast<int>(publish->topicNameLength));
		const QString payload = publish->payloadLength ? QString::fromUtf8(static_cast<const char*>(publish->pPayload), static_cast<int>(publish->payloadLength)) : QString{};
		self->received(topic, payload);
	}
	else if (packetInfo->type == MQTT_PACKET_TYPE_SUBACK || packetInfo->type == MQTT_PACKET_TYPE_UNSUBACK)
	{
		const auto* reasons = deserializedInfo->pReasonCode;
		if (!reasons || reasons->reasonCodeLength == 0)
			return true;

		for (size_t i = 0; i < reasons->reasonCodeLength; ++i)
		{
			const uint8_t code = reasons->reasonCode[i];
			const bool success = (packetInfo->type == MQTT_PACKET_TYPE_UNSUBACK) ? code == MQTT_REASON_UNSUBACK_SUCCESS
								: code == MQTT_REASON_SUBACK_GRANTED_QOS0 || code == MQTT_REASON_SUBACK_GRANTED_QOS1 || code == MQTT_REASON_SUBACK_GRANTED_QOS2;

			if (!success)
			{
				const char* packetType = packetInfo->type == MQTT_PACKET_TYPE_SUBACK ? "SUBACK" : "UNSUBACK";
				Error(self->_log, "{:s} rejected: packetId={:d}, reasonCode=0x{:02X}", packetType, deserializedInfo->packetIdentifier, code);
			}
			else if (packetInfo->type == MQTT_PACKET_TYPE_SUBACK && deserializedInfo->packetIdentifier == self->_apiSubscribePacketId)
			{
				Debug(self->_log, "MQTT API subscription accepted: packetId={:d}, qos={:d}", deserializedInfo->packetIdentifier, code);
			}
		}
	}
	else if (packetInfo->type == MQTT_PACKET_TYPE_DISCONNECT)
	{
		self->_brokerDisconnected = true;
		if (const auto* reason = deserializedInfo->pReasonCode; reason && reason->reasonCodeLength > 0 && reason->reasonCode[0] != MQTT_REASON_DISCONNECT_NORMAL_DISCONNECTION)
			Error(self->_log, "Broker disconnected: reasonCode=0x{:02X}", reason->reasonCode[0]);
		else
			Debug(self->_log, "Broker disconnected normally");
	}

	return true;
}

void mqtt::start(QString host, int port, QString username, QString password, bool is_ssl, bool ignore_ssl_errors, QString customTopic)
{
	if (_socket || _connecting || _connected)
		return;

	_stopping = false;
	_brokerDisconnected = false;
	_apiSubscribePacketId = MQTT_PACKET_ID_INVALID;
	_retryTimer->stop();
	_host = std::move(host);
	_port = port;
	_username = std::move(username);
	_password = std::move(password);
	_is_ssl = is_ssl;
	_ignore_ssl_errors = ignore_ssl_errors;
	_customTopic = std::move(customTopic);

	HYPERHDRAPI = QString(TEMPLATE_HYPERHDRAPI).arg(_customTopic);
	HYPERHDRAPI_RESPONSE = QString(TEMPLATE_HYPERHDRAPI_RESPONSE).arg(_customTopic);

	Debug(_log, "Starting MQTT connection. Address: {:s}:{:d}. Protocol: {:s}. Authentication: {:s}, Ignore SSL errors: {:s}", (_host), _port, _is_ssl ? "SSL" : "NO SSL", (!_username.isEmpty() || !_password.isEmpty()) ? "YES" : "NO", _ignore_ssl_errors ? "YES" : "NO");

	if (!_disableApiAccess)
		Debug(_log, "MQTT topic: {:s}, MQTT response: {:s}", (HYPERHDRAPI), (HYPERHDRAPI_RESPONSE));
	else
		Debug(_log, "MQTT access to HyperHDR API is disabled by user");

	_network = std::make_unique<NetworkContext>();
	_network->owner = this;

	_transport = {};
	_transport.recv = &mqtt::transportRecv;
	_transport.send = &mqtt::transportSend;
	_transport.writev = nullptr;
	_transport.pNetworkContext = _network.get();
	_fixedBuffer = { _networkBuffer.data(), _networkBuffer.size() };

	_connecting = true;
	auto status = MQTT_Init(&_mqttContext, &_transport, &mqtt::nowMs, &mqtt::eventCallback, &_fixedBuffer);
	if (status == MQTTSuccess)
		status = MQTT_InitStatefulQoS(&_mqttContext, _outgoingQos.data(), _outgoingQos.size(), _incomingQos.data(), _incomingQos.size(), nullptr, 0);

	if (status != MQTTSuccess)
	{
		error(status);
		transportFailure();
		return;
	}

	if (_is_ssl)
	{
		auto* socket = new QSslSocket(this);
		_socket = socket;

		if (_ignore_ssl_errors)
			connect(socket, &QSslSocket::sslErrors, socket, [socket](const QList<QSslError>&) { socket->ignoreSslErrors(); });

		connect(socket, &QSslSocket::encrypted, this, &mqtt::connectMqtt);
	}
	else
	{
		auto* socket = new QTcpSocket(this);
		_socket = socket;
		connect(socket, &QTcpSocket::connected, this, &mqtt::connectMqtt);
	}

	connect(_socket, &QAbstractSocket::readyRead, this, &mqtt::processLoop);
	connect(_socket, &QAbstractSocket::disconnected, this, [this] { disconnected(); detachSocket(); });
	connect(_socket, &QAbstractSocket::errorOccurred, this, [this]
		{
			if (!_stopping) {
				if (_socket)
					Error(_log, "MQTT transport error: {:s}", (_socket->errorString()));
				QMetaObject::invokeMethod(this, [this] { transportFailure(); }, Qt::QueuedConnection);
			}
		});

	if (_is_ssl)
		static_cast<QSslSocket*>(_socket)->connectToHostEncrypted(_host, static_cast<quint16>(_port));
	else
		static_cast<QTcpSocket*>(_socket)->connectToHost(_host, static_cast<quint16>(_port));
}

void mqtt::connectMqtt()
{
	if (!_socket || !_network || !_connecting)
		return;

	_network->socket = _socket;

	const QByteArray clientId = QStringLiteral("HyperHDR:%1").arg(QHostInfo::localHostName()).toUtf8();
	const QByteArray username = _username.toUtf8();
	const QByteArray password = _password.toUtf8();
	MQTTConnectInfo_t connectInfo{};
	connectInfo.cleanSession = true;
	connectInfo.keepAliveSeconds = 60;
	connectInfo.pClientIdentifier = clientId.constData();
	connectInfo.clientIdentifierLength = static_cast<size_t>(clientId.size());

	if (!username.isEmpty())
	{
		connectInfo.pUserName = username.constData();
		connectInfo.userNameLength = static_cast<size_t>(username.size());
	}
	if (!password.isEmpty())
	{
		connectInfo.pPassword = password.constData();
		connectInfo.passwordLength = static_cast<size_t>(password.size());
	}

	bool sessionPresent = false;
	_network->waitForData = true;
	const auto status = MQTT_Connect(&_mqttContext, &connectInfo, nullptr, ConnectTimeoutMs, &sessionPresent, nullptr, nullptr);
	_network->waitForData = false;
	if (status != MQTTSuccess) {
		error(status);
		transportFailure();
	}
	else {
		_connecting = false;
		connected();
		processLoop();
	}
}

void mqtt::processLoop()
{
	if (!_connected || !_socket || !_network)
		return;
	
	if (const auto status = MQTT_ProcessLoop(&_mqttContext); _brokerDisconnected)
	{
		transportFailure();

	}
	else if (status != MQTTSuccess && status != MQTTNeedMoreBytes)
	{
		error(status);
		transportFailure();
	}
	else if (_socket->bytesAvailable() > 0)
		QTimer::singleShot(0, this, &mqtt::processLoop);
}

void mqtt::connected()
{
	_connected = true;
	_currentRetry = 0;
	_retryTimer->stop();
	Debug(_log, "Connected");

	if (!_disableApiAccess)
		subscribe(HYPERHDRAPI, MQTTQoS2);

	if (!_connected)
		return;
	
	if (const int interval = keepAliveTimerMs(_mqttContext); interval > 0)
	{
		_processTimer->setInterval(interval);
		_processTimer->start();
	}
}

void mqtt::disconnected()
{
	const bool wasActive = _connected || _connecting;
	_connected = false;
	_connecting = false;
	_processTimer->stop();

	if (wasActive) {
		Debug(_log, "Disconnected");

		if (!_stopping)
			initRetry();
	}
}

void mqtt::transportFailure()
{
	if (!_stopping && (_socket || _network)) {
		disconnected();
		detachSocket();
		_network.reset();
		_brokerDisconnected = false;
	}
}

void mqtt::error(MQTTStatus_t status)
{
	Error(_log, "MQTT error: {:s}", (MQTT_Status_strerror(status)));
}

void mqtt::stop()
{
	_stopping = true;
	_processTimer->stop();
	_retryTimer->stop();
	_currentRetry = 0;

	if (_connected && _network && _network->socket && _network->socket->state() == QAbstractSocket::ConnectedState)
	{		
		if (const auto status = MQTT_Disconnect(&_mqttContext, nullptr, nullptr); status != MQTTSuccess)
			Error(_log, "MQTT disconnect failed: {:s}", (MQTT_Status_strerror(status)));

		_network->socket->flush();
		_network->socket->waitForBytesWritten(20);
	}

	_connected = false;
	_connecting = false;
	detachSocket();
	_network.reset();
	_brokerDisconnected = false;
}

void mqtt::detachSocket()
{
	_processTimer->stop();
	if (_socket)
	{		
		auto* socket = std::exchange(_socket, nullptr);
		QObject::disconnect(socket, nullptr, this, nullptr);
		socket->abort();
		socket->deleteLater();
	}

	if (_network)
		_network->socket = nullptr;
}

void mqtt::initRetry()
{
	if (_maxRetry <= 0 || _retryTimer->isActive() || _connected)
		return;

	Debug(_log, "Prepare to retry in {:d} seconds (limit: {:d})", RetryIntervalMs / 1000, _maxRetry);
	_retryTimer->start(RetryIntervalMs);
}

void mqtt::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type != settings::type::MQTT)
		return;

	const QJsonObject object = config.object();
	_enabled = object["enable"].toBool(false);
	_host = object["host"].toString();
	_port = object["port"].toInt(1883);
	_username = object["username"].toString();
	_password = object["password"].toString();
	_is_ssl = object["is_ssl"].toBool(false);
	_ignore_ssl_errors = object["ignore_ssl_errors"].toBool(true);
	_maxRetry = object["maxRetry"].toInt(120);
	_disableApiAccess = object["disableApiAccess"].toBool(false);
	_customTopic = object["custom_topic"].toString().trimmed();
	if (_customTopic.isEmpty())
		_customTopic = "HyperHDR";

	if (_initialized)
	{
		stop();
		if (_enabled)
			start(_host, _port, _username, _password, _is_ssl, _ignore_ssl_errors, _customTopic);
	}

	_initialized = true;
}

void mqtt::begin()
{
	if (_initialized && _enabled)
		start(_host, _port, _username, _password, _is_ssl, _ignore_ssl_errors, _customTopic);
}

void mqtt::handleSignalMqttSubscribe(bool subscribeTopic, QString topic)
{
	if (!_connected)
		return;

	if (subscribeTopic)
		subscribe(topic, MQTTQoS0);
	else
		unsubscribe(topic);
}

void mqtt::handleSignalMqttPublish(QString topic, QString payload)
{
	if (_connected)
		publish(topic, payload);
}

void mqtt::subscribe(const QString& topic, MQTTQoS_t qos)
{
	const QByteArray filter = topic.toUtf8();
	MQTTSubscribeInfo_t subscription{};
	subscription.qos = qos;
	subscription.pTopicFilter = filter.constData();
	subscription.topicFilterLength = static_cast<size_t>(filter.size());
	subscription.retainHandlingOption = retainSendOnSub;

	const uint16_t packetId = MQTT_GetPacketId(&_mqttContext);
	
	if (const auto status = MQTT_Subscribe(&_mqttContext, &subscription, 1, packetId, nullptr); status != MQTTSuccess)
	{
		error(status);
		if (status == MQTTSendFailed)
			transportFailure();
	}
	else if (topic == HYPERHDRAPI)
	{
		_apiSubscribePacketId = packetId;
	}
}

void mqtt::unsubscribe(const QString& topic)
{
	const QByteArray filter = topic.toUtf8();
	MQTTSubscribeInfo_t subscription{};
	subscription.pTopicFilter = filter.constData();
	subscription.topicFilterLength = static_cast<size_t>(filter.size());
	subscription.retainHandlingOption = retainDoNotSendonSub;
	
	if (const auto status = MQTT_Unsubscribe(&_mqttContext, &subscription, 1, MQTT_GetPacketId(&_mqttContext), nullptr); status != MQTTSuccess)
	{
		error(status);
		if (status == MQTTSendFailed)
			transportFailure();
	}
}

void mqtt::publish(const QString& topic, const QString& payload, MQTTQoS_t qos)
{
	if (!_connected)
		return;

	const QByteArray topicUtf8 = topic.toUtf8();
	const QByteArray payloadUtf8 = payload.toUtf8();
	MQTTPublishInfo_t publishInfo{};
	publishInfo.qos = qos;
	publishInfo.pTopicName = topicUtf8.constData();
	publishInfo.topicNameLength = static_cast<size_t>(topicUtf8.size());
	publishInfo.pPayload = payloadUtf8.constData();
	publishInfo.payloadLength = static_cast<size_t>(payloadUtf8.size());

	const uint16_t packetId = qos == MQTTQoS0 ? MQTT_PACKET_ID_INVALID : MQTT_GetPacketId(&_mqttContext);	
	if (const auto status = MQTT_Publish(&_mqttContext, &publishInfo, packetId, nullptr); status != MQTTSuccess)
	{
		error(status);
		if (status == MQTTSendFailed)
			transportFailure();
	}
}

void mqtt::received(const QString& topic, const QString& payload)
{
	if (topic == HYPERHDRAPI && !payload.isEmpty())
	{
		QJsonParseError parseError;
		const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
		QString response;

		if (parseError.error != QJsonParseError::NoError){
			QJsonObject errorObject;
			errorObject["success"] = false;
			errorObject["error"] = QStringLiteral("%1 at offset: %2").arg(parseError.errorString()).arg(parseError.offset);
			response = QJsonDocument(errorObject).toJson(QJsonDocument::Compact);
		}
		else{
			QJsonDocument result;
			executeJson(QStringLiteral("MQTT"), document, result);
			response = result.toJson(QJsonDocument::Compact);
			Debug(_log, "JSON result: {:s}", (response));
		}

		publish(HYPERHDRAPI_RESPONSE, response, MQTTQoS2);
		return;
	}

	emit GlobalSignals::getInstance()->SignalMqttReceived(topic, payload);
}

void mqtt::executeJson(const QString& origin, const QJsonDocument& input, QJsonDocument& result)
{
	if (_disableApiAccess)
	{
		Error(_log, "API access is disabled in MQTT configuration");
		return;
	}

	auto* hyperAPI = new HyperAPI(origin, _log, true, this);
	_resultArray = {};
	connect(hyperAPI, &HyperAPI::SignalCallbackJsonMessage, this, [this](const QJsonObject& response) { _resultArray.append(response); });
	hyperAPI->initialize();

	if (input.isObject())
	{
		hyperAPI->handleMessage(input.toJson());
	}
	else if (input.isArray())
	{
		for (const auto& element : input.array())
			if (element.isObject())
				hyperAPI->handleMessage(QJsonDocument(element.toObject()).toJson());
	}

	result.setArray(_resultArray);
	disconnect(hyperAPI, &HyperAPI::SignalCallbackJsonMessage, this, nullptr);
	hyperAPI->deleteLater();
}

void mqtt::handleSignalMqttLastWill(QString id, QStringList pairs)
{
	if (!id.isEmpty())
	{
		if (!pairs.isEmpty() && (pairs.size() % 2) == 0)
			_lastWill.insert(id, std::move(pairs));
		else 
			_lastWill.remove(id);
	}
	else
	{
		for (auto it = _lastWill.cbegin(); it != _lastWill.cend(); ++it)
		{
			const auto& messages = it.value();
			for (qsizetype i = 0; i + 1 < messages.size(); i += 2)
				publish(messages.at(i), messages.at(i + 1));
		}

		_lastWill.clear();
	}
}

///////////////////////////////////////////////////////////
/////////////////////// Testing ///////////////////////////
///////////////////////////////////////////////////////////
// listener:
// mosquitto_sub -h localhost -t HyperHDR/JsonAPI/response
// commands:
// mosquitto_pub -h localhost -t HyperHDR/JsonAPI -m "[{\"command\" : \"clear\", \"priority\" :1}, {\"command\" : \"clear\", \"priority\" :2}]"
// mosquitto_pub -h localhost -t HyperHDR/JsonAPI -m "[{\"command\":\"componentstate\",\"componentstate\": {\"component\":\"HDR\",\"state\": true } }, {\"command\":\"componentstate\",\"componentstate\": {\"component\":\"HDR\",\"state\": false } }]"
// mosquitto_pub -h localhost -t HyperHDR/JsonAPI -m "[{\"command\" : \"instance\",\"subcommand\":\"switchTo\",\"instance\":1},{\"command\":\"componentstate\",\"componentstate\":{\"component\":\"LEDDEVICE\",\"state\": false}}]"
///////////////////////////////////////////////////////////
