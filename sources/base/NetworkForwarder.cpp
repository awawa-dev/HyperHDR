/* NetworkForwarder.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#ifndef PCH_ENABLED
	#include <QThread>
	#include <stdexcept>

	#include <utils/Logger.h>
#endif

#include <QTcpServer>
#include <QTcpSocket>

#include <base/NetworkForwarder.h>
#include <base/HyperHdrInstance.h>
#include <flatbuffers/client/FlatBuffersClient.h>
#include <utils/GlobalSignals.h>
#include <base/HyperHdrManager.h>
#include <base/Muxer.h>

NetworkForwarder::NetworkForwarder()
	: QObject(nullptr)
	, _log(Logger::getInstance("NETFORWARDER"))
	, _forwarderEnabled(false)
	, _priority(140)
	, _hasImage(false)
	, _image()
{
}

void NetworkForwarder::startedHandler()
{
	std::shared_ptr<HyperHdrManager> instanceManager;
	emit GlobalSignals::getInstance()->SignalGetInstanceManager(instanceManager);
	if (instanceManager == nullptr)
	{
		Error(_log, "Instance manager is already removed");
		return;
	}

	std::shared_ptr<HyperHdrInstance> hyperhdr;	
	SAFE_CALL_1_RET(instanceManager.get(), getHyperHdrInstance, std::shared_ptr<HyperHdrInstance>, hyperhdr, quint8, 0);
	if (hyperhdr == nullptr)
	{
		Error(_log, "Could not get first instance");
		return;
	}

	_instanceZero = hyperhdr;

	connect(this, &NetworkForwarder::SignalForwardImage, this, &NetworkForwarder::signalForwardImageHandler, Qt::QueuedConnection);
	connect(hyperhdr.get(), &HyperHdrInstance::SignalInstanceSettingsChanged, this, &NetworkForwarder::handleSettingsUpdate);
	connect(hyperhdr.get(), &HyperHdrInstance::SignalRequestComponent, this, &NetworkForwarder::handleCompStateChangeRequest);
	connect(hyperhdr.get(), &HyperHdrInstance::SignalColorIsSet, this, &NetworkForwarder::signalColorIsSetHandler);
	

	handleCompStateChangeRequest(hyperhdr::COMP_FORWARDER, true);
}

NetworkForwarder::~NetworkForwarder()
{
	auto hyperhdr = _instanceZero.lock();
	if (hyperhdr != nullptr)
	{
		disconnect(hyperhdr.get(), nullptr, this, nullptr);
	}

	Debug(_log, "NetworkForwarder has been removed");
}

void NetworkForwarder::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::NETFORWARD)
	{
		auto hyperhdr = _instanceZero.lock();
		if (hyperhdr == nullptr)
			return;

		_jsonSlaves.clear();
		_flatSlaves.clear();		
		while (!_forwardClients.isEmpty())
			_forwardClients.takeFirst()->deleteLater();

		disconnect(hyperhdr.get(), &HyperHdrInstance::SignalForwardJsonMessage, this, nullptr);
		disconnect(hyperhdr.get(), &HyperHdrInstance::SignalInstanceImageUpdated, this, nullptr);
		disconnect(GlobalSignals::getInstance(), nullptr, this, nullptr);

		_hasImage = false;
		_image = Image<ColorRgb>();

		const QJsonObject& obj = config.object();

		if (!obj["enable"].toBool() || !_forwarderEnabled)
		{
			_forwarderEnabled = false;
			QUEUE_CALL_2(hyperhdr.get(), setNewComponentState,hyperhdr::Components, hyperhdr::COMP_FORWARDER, bool, _forwarderEnabled);
			return;
		}

		if (!obj["json"].isNull())
		{
			QJsonDocument jsonConf;
			SAFE_CALL_1_RET(hyperhdr.get(), getSetting, QJsonDocument, jsonConf, settings::type, settings::type::JSONSERVER);
			for (auto&& entry : obj["json"].toArray())
			{
				addJsonSlave(entry.toString(), jsonConf.object());
			}
		}

		if (!obj["flat"].isNull())
		{
			QJsonDocument flatConf;
			SAFE_CALL_1_RET(hyperhdr.get(), getSetting, QJsonDocument, flatConf, settings::type, settings::type::FLATBUFSERVER);
			for (auto&& entry : obj["flat"].toArray())
			{
				addFlatbufferSlave(entry.toString(), flatConf.object());
			}
		}

		if (!_jsonSlaves.isEmpty())
		{
			Info(_log, "Forward now to json targets '%s'", QSTRING_CSTR(_jsonSlaves.join(", ")));
			connect(hyperhdr.get(), &HyperHdrInstance::SignalForwardJsonMessage, this, &NetworkForwarder::forwardJsonMessage, Qt::UniqueConnection);
		}			

		if (!_flatSlaves.isEmpty())
		{
			connect(hyperhdr.get(), &HyperHdrInstance::SignalInstanceImageUpdated, this, &NetworkForwarder::handlerInstanceImageUpdated, Qt::DirectConnection);
		}

		QUEUE_CALL_2(hyperhdr.get(), setNewComponentState, hyperhdr::Components, hyperhdr::COMP_FORWARDER, bool, _forwarderEnabled);
	}
}

void NetworkForwarder::handleCompStateChangeRequest(hyperhdr::Components component, bool enable)
{
	if (component == hyperhdr::COMP_FORWARDER && _forwarderEnabled != enable)
	{
		auto hyperhdr = _instanceZero.lock();
		if (hyperhdr == nullptr)
			return;		

		QJsonDocument netForConf;
		SAFE_CALL_1_RET(hyperhdr.get(), getSetting, QJsonDocument, netForConf, settings::type, settings::type::NETFORWARD);

		_forwarderEnabled = enable;
		handleSettingsUpdate(settings::type::NETFORWARD, netForConf);
		Info(_log, "Forwarder has changed state to %s", (_forwarderEnabled ? "enabled" : "disabled"));
	}
}

void NetworkForwarder::addJsonSlave(const QString& slave, const QJsonObject& obj)
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
	if (QHostAddress(parts[0]) == QHostAddress::LocalHost && parts[1].toInt() == obj["port"].toInt())
	{
		Error(_log, "Loop between JsonServer and Forwarder! (%s)", QSTRING_CSTR(slave));
		return;
	}

	if (_forwarderEnabled && !_jsonSlaves.contains(slave))
		_jsonSlaves << slave;
}

void NetworkForwarder::addFlatbufferSlave(const QString& slave, const QJsonObject& obj)
{
	if (slave != HYPERHDR_DOMAIN_SERVER)
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
		if (QHostAddress(parts[0]) == QHostAddress::LocalHost && parts[1].toInt() == obj["port"].toInt())
		{
			Error(_log, "Loop between Flatbuffer Server and Forwarder! (%s)", QSTRING_CSTR(slave));
			return;
		}
	}

	if (_forwarderEnabled && !_flatSlaves.contains(slave))
	{
		FlatBuffersClient* flatbuf = new FlatBuffersClient(this, "Forwarder", slave, _priority, false);
		QString anyError = flatbuf->getErrorString();

		if (anyError.isEmpty())
		{
			_flatSlaves << slave;
			_forwardClients << flatbuf;
		}
		else
		{
			Error(_log, "Could not initialize client: %s", QSTRING_CSTR(anyError));
			flatbuf->deleteLater();
		}
	}
}

void NetworkForwarder::forwardJsonMessage(const QJsonObject& message)
{
	if (_forwarderEnabled)
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

void NetworkForwarder::signalColorIsSetHandler(ColorRgb color, int duration)
{
	for (int i = 0; i < _forwardClients.size(); i++)
	{
		emit _forwardClients.at(i)->SignalSetColor(color, duration);
	}
}

void NetworkForwarder::handlerInstanceImageUpdated(const Image<ColorRgb>& image)
{
	if (_hasImage.exchange(true))
		return;
	
	_image = image;

	emit SignalForwardImage();
}

void NetworkForwarder::signalForwardImageHandler()
{
	if (!_hasImage.exchange(false))
		return;

	for (int i = 0; i < _forwardClients.size(); i++)
	{
		emit _forwardClients.at(i)->SignalImageToSend(_image);
	}
	
	_image = Image<ColorRgb>();
}

void NetworkForwarder::sendJsonMessage(const QJsonObject& message, QTcpSocket* socket)
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
