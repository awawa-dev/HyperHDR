// STL includes
#include <stdexcept>

// project includes
#include <hyperhdrbase/MessageForwarder.h>


#include <hyperhdrbase/HyperHdrInstance.h>

// utils includes
#include <utils/Logger.h>

// qt includes
#include <QTcpServer>
#include <QTcpSocket>

#include <flatbufserver/FlatBufferConnection.h>

MessageForwarder::MessageForwarder(HyperHdrInstance* hyperhdr)
	: QObject()
	, _hyperhdr(hyperhdr)
	, _log(Logger::getInstance("NETFORWARDER"))
	, _muxer(_hyperhdr->getMuxerInstance())
	, _forwarder_enabled(true)
	, _priority(140)
{
	// get settings updates
	connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &MessageForwarder::handleSettingsUpdate);

	// component changes
	connect(_hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &MessageForwarder::handleCompStateChangeRequest);

	// connect with Muxer visible priority changes
	connect(_muxer, &PriorityMuxer::visiblePriorityChanged, this, &MessageForwarder::handlePriorityChanges);

	// init
	handleSettingsUpdate(settings::type::NETFORWARD, _hyperhdr->getSetting(settings::type::NETFORWARD));
}

MessageForwarder::~MessageForwarder()
{
	while (!_forwardClients.isEmpty())
		delete _forwardClients.takeFirst();
}

void MessageForwarder::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::NETFORWARD)
	{
		// clear the current targets
		_jsonSlaves.clear();
		_flatSlaves.clear();
		while (!_forwardClients.isEmpty())
			delete _forwardClients.takeFirst();

		// build new one
		const QJsonObject& obj = config.object();
		if (!obj["json"].isNull())
		{
			const QJsonArray& addr = obj["json"].toArray();
			for (auto&& entry : addr)
			{
				addJsonSlave(entry.toString());
			}
		}

		if (!obj["flat"].isNull())
		{
			const QJsonArray& addr = obj["flat"].toArray();
			for (auto&& entry : addr)
			{
				addFlatbufferSlave(entry.toString());
			}
		}

		if (!_jsonSlaves.isEmpty() && obj["enable"].toBool() && _forwarder_enabled)
		{
			InfoIf(obj["enable"].toBool(true), _log, "Forward now to json targets '%s'", QSTRING_CSTR(_jsonSlaves.join(", ")));
			connect(_hyperhdr, &HyperHdrInstance::forwardJsonMessage, this, &MessageForwarder::forwardJsonMessage, Qt::UniqueConnection);
		}
		else if (_jsonSlaves.isEmpty() || !obj["enable"].toBool() || !_forwarder_enabled)
			disconnect(_hyperhdr, &HyperHdrInstance::forwardJsonMessage, 0, 0);

		if (!_flatSlaves.isEmpty() && obj["enable"].toBool() && _forwarder_enabled)
		{
			InfoIf(obj["enable"].toBool(true), _log, "Forward now to flatbuffer targets '%s'", QSTRING_CSTR(_flatSlaves.join(", ")));

			hyperhdr::Components activeCompId = _hyperhdr->getPriorityInfo(_hyperhdr->getCurrentPriority()).componentId;

			disconnect(_hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage, 0, 0);
			disconnect(_hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage, 0, 0);

			if (activeCompId == hyperhdr::COMP_SYSTEMGRABBER)
			{
				connect(_hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage, this, &MessageForwarder::forwardFlatbufferMessage, Qt::UniqueConnection);
			}
			else  if (activeCompId == hyperhdr::COMP_VIDEOGRABBER)
			{				
				connect(_hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage, this, &MessageForwarder::forwardFlatbufferMessage, Qt::UniqueConnection);
			}
		}
		else if (_flatSlaves.isEmpty() || !obj["enable"].toBool() || !_forwarder_enabled)
		{
			disconnect(_hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage, 0, 0);
			disconnect(_hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage, 0, 0);
		}

		// update comp state
		_hyperhdr->setNewComponentState(hyperhdr::COMP_FORWARDER, obj["enable"].toBool(true));
	}
}

void MessageForwarder::handleCompStateChangeRequest(hyperhdr::Components component, bool enable)
{
	if (component == hyperhdr::COMP_FORWARDER && _forwarder_enabled != enable)
	{
		_forwarder_enabled = enable;
		handleSettingsUpdate(settings::type::NETFORWARD, _hyperhdr->getSetting(settings::type::NETFORWARD));
		Info(_log, "Forwarder change state to %s", (_forwarder_enabled ? "enabled" : "disabled"));
		_hyperhdr->setNewComponentState(component, _forwarder_enabled);
	}
}

void MessageForwarder::handlePriorityChanges(quint8 priority)
{
	const QJsonObject obj = _hyperhdr->getSetting(settings::type::NETFORWARD).object();
	if (priority != 0 && _forwarder_enabled && obj["enable"].toBool())
	{
		//_flatSlaves.clear();
		//while (!_forwardClients.isEmpty())
		//	delete _forwardClients.takeFirst();

		hyperhdr::Components activeCompId = _hyperhdr->getPriorityInfo(priority).componentId;
		if (activeCompId == hyperhdr::COMP_SYSTEMGRABBER || activeCompId == hyperhdr::COMP_VIDEOGRABBER)
		{
			if (!obj["flat"].isNull())
			{
				const QJsonArray& addr = obj["flat"].toArray();
				for (auto&& entry : addr)
				{
					addFlatbufferSlave(entry.toString());
				}
			}

			switch (activeCompId)
			{
			case hyperhdr::COMP_SYSTEMGRABBER:
			{
				disconnect(_hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage, 0, 0);
				connect(_hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage, this, &MessageForwarder::forwardFlatbufferMessage, Qt::UniqueConnection);
			}
			break;
			case hyperhdr::COMP_VIDEOGRABBER:
			{
				disconnect(_hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage, 0, 0);
				connect(_hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage, this, &MessageForwarder::forwardFlatbufferMessage, Qt::UniqueConnection);
			}
			break;
			default:
			{
				disconnect(_hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage, 0, 0);
				disconnect(_hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage, 0, 0);
			}
			}
		}
		else
		{
			disconnect(_hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage, 0, 0);
			disconnect(_hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage, 0, 0);
		}
	}
}

void MessageForwarder::addJsonSlave(const QString& slave)
{
	QStringList parts = slave.split(":");
	if (parts.size() != 2)
	{
		Error(_log, "Unable to parse address (%s)", QSTRING_CSTR(slave));
		return;
	}

	bool ok;
	parts[1].toUShort(&ok);
	if (!ok)
	{
		Error(_log, "Unable to parse port number (%s)", QSTRING_CSTR(parts[1]));
		return;
	}

	// verify loop with jsonserver
	const QJsonObject& obj = _hyperhdr->getSetting(settings::type::JSONSERVER).object();
	if (QHostAddress(parts[0]) == QHostAddress::LocalHost && parts[1].toInt() == obj["port"].toInt())
	{
		Error(_log, "Loop between JsonServer and Forwarder! (%s)", QSTRING_CSTR(slave));
		return;
	}

	if (_forwarder_enabled && !_jsonSlaves.contains(slave))
		_jsonSlaves << slave;
}

void MessageForwarder::addFlatbufferSlave(const QString& slave)
{
	QStringList parts = slave.split(":");
	if (parts.size() != 2)
	{
		Error(_log, "Unable to parse address (%s)", QSTRING_CSTR(slave));
		return;
	}

	bool ok;
	parts[1].toUShort(&ok);
	if (!ok)
	{
		Error(_log, "Unable to parse port number (%s)", QSTRING_CSTR(parts[1]));
		return;
	}

	// verify loop with flatbufserver
	const QJsonObject& obj = _hyperhdr->getSetting(settings::type::FLATBUFSERVER).object();
	if (QHostAddress(parts[0]) == QHostAddress::LocalHost && parts[1].toInt() == obj["port"].toInt())
	{
		Error(_log, "Loop between Flatbuffer Server and Forwarder! (%s)", QSTRING_CSTR(slave));
		return;
	}

	if (_forwarder_enabled && !_flatSlaves.contains(slave))
	{
		_flatSlaves << slave;
		FlatBufferConnection* flatbuf = new FlatBufferConnection("Forwarder", slave.toLocal8Bit().constData(), _priority, false);
		_forwardClients << flatbuf;
	}
}

void MessageForwarder::forwardJsonMessage(const QJsonObject& message)
{
	if (_forwarder_enabled)
	{
		QTcpSocket client;
		for (int i = 0; i < _jsonSlaves.size(); i++)
		{
			QStringList parts = _jsonSlaves.at(i).split(":");
			client.connectToHost(QHostAddress(parts[0]), parts[1].toUShort());
			if (client.waitForConnected(500))
			{
				sendJsonMessage(message, &client);
				client.close();
			}
		}
	}
}

void MessageForwarder::forwardFlatbufferMessage(const QString& name, const Image<ColorRgb>& image)
{
	if (_forwarder_enabled)
	{
		for (int i = 0; i < _forwardClients.size(); i++)
			if (_forwardClients.at(i)->isFree())
				emit _forwardClients.at(i)->onImage(image);
	}
}

void MessageForwarder::sendJsonMessage(const QJsonObject& message, QTcpSocket* socket)
{
	QJsonObject jsonMessage = message;
	if (jsonMessage.contains("tan") && jsonMessage["tan"].isNull())
		jsonMessage["tan"] = 100;

	// serialize message
	QJsonDocument writer(jsonMessage);
	QByteArray serializedMessage = writer.toJson(QJsonDocument::Compact) + "\n";

	// write message
	socket->write(serializedMessage);
	if (!socket->waitForBytesWritten())
	{
		Debug(_log, "Error while writing data to host");
		return;
	}

	// read reply data
	QByteArray serializedReply;
	while (!serializedReply.contains('\n'))
	{
		// receive reply
		if (!socket->waitForReadyRead())
		{
			Debug(_log, "Error while writing data from host");
			return;
		}

		serializedReply += socket->readAll();
	}

	// parse reply data
	QJsonParseError error;
	QJsonDocument reply = QJsonDocument::fromJson(serializedReply, &error);

	if (error.error != QJsonParseError::NoError)
	{
		Error(_log, "Error while parsing reply: invalid json");
		return;
	}
}
