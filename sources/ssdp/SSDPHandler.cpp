#include <ssdp/SSDPHandler.h>

#include <webserver/WebServer.h>
#include "SSDPDescription.h"
#include <base/HyperHdrInstance.h>
#include <HyperhdrConfig.h>
#include <base/AccessManager.h>
#include <QNetworkInterface>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
	#include <QRandomGenerator>
#endif

#define DEFAULT_RETRY 10

namespace {
	constexpr const char* SSDP_IDENTIFIER = "urn:hyperhdr.eu:device:basic:1";
}

SSDPHandler::SSDPHandler(QString uuid, quint16 flatBufPort, quint16 protoBufPort, quint16 jsonServerPort, quint16 sslPort, quint16 webPort, const QString& name, QObject* parent)
	: SSDPServer(parent)
	, _log("SSDP")
	, _localAddress()
	, _uuid(uuid)
	, _retry(DEFAULT_RETRY)
{
	setFlatBufPort(flatBufPort);
	setProtoBufPort(protoBufPort);
	setJsonServerPort(jsonServerPort);
	setSSLServerPort(sslPort);
	setWebServerPort(webPort);
	setHyperhdrName(name);
	Debug(_log, "SSDPHandler is initialized");
}

SSDPHandler::~SSDPHandler()
{
	stopServer();
	Debug(_log, "SSDPHandler is closed");
}

void SSDPHandler::initServer()
{
	Debug(_log, "SSDPHandler is initializing");

	if (_localAddress.isEmpty())
	{
		_localAddress = getLocalAddress();
	}

	if (_localAddress.isEmpty())
	{
		if (_retry > 0)
		{
			Warning(_log, "Could not obtain the local address. Retry later ({:d}/{:d})", (DEFAULT_RETRY - _retry + 1), DEFAULT_RETRY);
			QTimer::singleShot(30000, this, [this]() {
				if (!_running && _localAddress.isEmpty() && _retry > 0)
					initServer();				
			});			
			_retry--;
		}
		else
			Warning(_log, "Could not obtain the local address.");
	}
	else
		_retry = 0;

	SSDPServer::setUuid(_uuid);

	// announce targets
	_deviceList.push_back("upnp:rootdevice");
	_deviceList.push_back("uuid:" + _uuid);
	_deviceList.push_back(SSDP_IDENTIFIER);

	// prep server
	SSDPServer::initServer();

	// listen for mSearchRequestes
	connect(this, &SSDPServer::msearchRequestReceived, this, &SSDPHandler::handleMSearchRequest);

	handleWebServerStateChange(true);	
}

void SSDPHandler::stopServer()
{
	sendAnnounceList(false);
	SSDPServer::stop();
}


void SSDPHandler::handleWebServerStateChange(bool newState)
{
	if (_localAddress.isEmpty())
	{
		Debug(_log, "The local address is empty");
		return;
	}

	if (newState)
	{
		// refresh info
		QString param = buildDesc();
		emit newSsdpXmlDesc(param);
		setDescriptionAddress(getDescAddress());
		if (start())
			sendAnnounceList(true);
		else
			Warning(_log, "Could not start the SSDP server");
	}
	else
	{				
		emit newSsdpXmlDesc("");
		sendAnnounceList(false);
		stop();
	}
}

void SSDPHandler::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	const QJsonObject& obj = config.object();

	if (type == settings::type::FLATBUFSERVER)
	{
		if (obj["port"].toInt() != SSDPServer::getFlatBufPort())
		{
			SSDPServer::setFlatBufPort(obj["port"].toInt());
		}
	}

	if (type == settings::type::PROTOSERVER)
	{
		if (obj["port"].toInt() != SSDPServer::getProtoBufPort())
		{
			SSDPServer::setProtoBufPort(obj["port"].toInt());
		}
	}

	if (type == settings::type::JSONSERVER)
	{
		if (obj["port"].toInt() != SSDPServer::getJsonServerPort())
		{
			SSDPServer::setJsonServerPort(obj["port"].toInt());
		}
	}

	if (type == settings::type::WEBSERVER)
	{
		if (obj["sslPort"].toInt() != SSDPServer::getSSLServerPort())
		{
			SSDPServer::setSSLServerPort(obj["sslPort"].toInt());
		}

		if (obj["port"].toInt() != SSDPServer::getWebServerPort())
		{
			SSDPServer::setWebServerPort(obj["port"].toInt());
		}
	}

	if (type == settings::type::GENERAL)
	{
		if (obj["name"].toString() != SSDPServer::getHyperhdrName())
		{
			SSDPServer::setHyperhdrName(obj["name"].toString());
		}
	}
}

QString SSDPHandler::getLocalAddress() const
{
	for (const auto& _interface : QNetworkInterface::allInterfaces())
	{
		if ((_interface.type() != QNetworkInterface::InterfaceType::Ethernet &&
			_interface.type() != QNetworkInterface::InterfaceType::Wifi) ||
			_interface.humanReadableName().indexOf("virtual", 0, Qt::CaseInsensitive) >= 0 ||
			!_interface.flags().testFlag(QNetworkInterface::InterfaceFlag::IsRunning) ||
			!_interface.flags().testFlag(QNetworkInterface::InterfaceFlag::IsUp))
			continue;

		for (const auto& addressEntr : _interface.addressEntries())
		{
			auto address = addressEntr.ip();
			if (!address.isLoopback() && address.protocol() == QAbstractSocket::IPv4Protocol)
			{
				QString retVal = address.toString();
				Debug(_log, "The local address is: {:s}", (retVal));
				return retVal;
			}
		}
	}

	// get the first valid IPv4 address. This is probably not that one we actually want to announce
	for (const auto& address : QNetworkInterface::allAddresses())
	{
		// is valid when, no loopback, IPv4
		if (!address.isLoopback() && address.protocol() == QAbstractSocket::IPv4Protocol)
		{
			QString retVal = address.toString();
			Debug(_log, "The local address is: {:s}", (retVal));
			return retVal;
		}
	}
	return QString();
}

void SSDPHandler::handleMSearchRequest(const QString& target, const QString& mx, const QString address, quint16 port)
{
	const auto respond = [this, target, address, port]() {
		// when searched for all devices / root devices / basic device
		if (target == "ssdp:all")
			sendMSearchResponse(SSDP_IDENTIFIER, address, port);
		else if (target == "upnp:rootdevice" || target == "urn:schemas-upnp-org:device:basic:1" || target == SSDP_IDENTIFIER)
			sendMSearchResponse(target, address, port);
	};

	bool ok = false;
	int maxDelay = mx.toInt(&ok);
	if (ok)
	{
		/* Pick a random delay between 0 and MX seconds */
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
		int randomDelay = QRandomGenerator::global()->generate() % (maxDelay * 1000);
#else
		int randomDelay = qrand() % (maxDelay * 1000);
#endif
		QTimer::singleShot(randomDelay, this, respond);
	}
	else
	{
		/* MX Header is not valid.
		 * Send response without delay */
		respond();
	}
}

QString SSDPHandler::getDescAddress() const
{
	return getBaseAddress() + "description.xml";
}

QString SSDPHandler::getBaseAddress() const
{
	return QString("http://%1:%2/").arg(_localAddress).arg(SSDPServer::getWebServerPort());
}

QString SSDPHandler::buildDesc() const
{
	/// %1 base url                   http://192.168.1.26:8090/
	/// %2 friendly name              HyperHDR (192.168.1.26)
	/// %3 modelNumber                17.0.0
	/// %4 serialNumber / UDN (H ID)  Fjsa723dD0....
	/// %5 json port                  19444
	/// %6 ssl server port            8092
	/// %7 protobuf port              19445
	/// %8 flatbuf port               19400

	return QString(SSDP_DESCRIPTION).arg(
		getBaseAddress(),
		QString("HyperHDR (%1)").arg(_localAddress),
		QString(HYPERHDR_VERSION),
		_uuid,
		QString::number(SSDPServer::getJsonServerPort()),
		QString::number(SSDPServer::getSSLServerPort()),
		QString::number(SSDPServer::getProtoBufPort()),
		QString::number(SSDPServer::getFlatBufPort())
	);
}

void SSDPHandler::sendAnnounceList(bool alive)
{
	for (const auto& entry : _deviceList) {
		alive ? SSDPServer::sendAlive(entry) : SSDPServer::sendByeBye(entry);
	}
}

