#include <ssdp/SSDPServer.h>

// HyperHDR
#include <HyperhdrConfig.h>

#include <QUdpSocket>
#include <QDateTime>

static const QHostAddress SSDP_ADDR("239.255.255.250");
static const quint16      SSDP_PORT(1900);
static const QString      SSDP_MAX_AGE("1800");

// as per upnp spec 1.1, section 1.2.2.
//  - BOOTID.UPNP.ORG
//  - CONFIGID.UPNP.ORG
//  - SEARCHPORT.UPNP.ORG (optional)
// TODO: Make IP and port below another #define and replace message below
static const QString UPNP_ALIVE_MESSAGE =	"NOTIFY * HTTP/1.1\r\n"
											"HOST: 239.255.255.250:1900\r\n"
											"CACHE-CONTROL: max-age=%1\r\n"
											"LOCATION: %2\r\n"
											"NT: %3\r\n"
											"NTS: ssdp:alive\r\n"
											"SERVER: %4\r\n"
											"USN: uuid:%5\r\n"
											"HYPERHDR-FBS-PORT: %6\r\n"
											"HYPERHDR-JSS-PORT: %7\r\n"
											"HYPERHDR-NAME: %8\r\n"
											"\r\n";

// Implement ssdp:update as per spec 1.1, section 1.2.4
// and use the below define to build the message, where
// SEARCHPORT.UPNP.ORG are optional.
// TODO: Make IP and port below another #define and replace message below
static const QString UPNP_UPDATE_MESSAGE =	"NOTIFY * HTTP/1.1\r\n"
											"HOST: 239.255.255.250:1900\r\n"
											"LOCATION: %1\r\n"
											"NT: %2\r\n"
											"NTS: ssdp:update\r\n"
											"USN: uuid:%3\r\n"
/*                                         "CONFIGID.UPNP.ORG: %4\r\n"
UPNP spec = 1.1                            "NEXTBOOTID.UPNP.ORG: %5\r\n"
										   "SEARCHPORT.UPNP.ORG: %6\r\n"
*/                                         "\r\n";

// TODO: Add this two fields commented below in the BYEBYE MESSAGE
// as per upnp spec 1.1, section 1.2.2 and 1.2.3.
//  - BOOTID.UPNP.ORG
//  - CONFIGID.UPNP.ORG
// TODO: Make IP and port below another #define and replace message below
static const QString UPNP_BYEBYE_MESSAGE =	"NOTIFY * HTTP/1.1\r\n"
											"HOST: 239.255.255.250:1900\r\n"
											"NT: %1\r\n"
											"NTS: ssdp:byebye\r\n"
											"USN: uuid:%2\r\n"
											"\r\n";

// TODO: Add this three fields commented below in the MSEARCH_RESPONSE
// as per upnp spec 1.1, section 1.3.3.
//  - BOOTID.UPNP.ORG
//  - CONFIGID.UPNP.ORG
//  - SEARCHPORT.UPNP.ORG (optional)
static const QString UPNP_MSEARCH_RESPONSE ="HTTP/1.1 200 OK\r\n"
											"CACHE-CONTROL: max-age = %1\r\n"
											"DATE: %2\r\n"
											"EXT: \r\n"
											"LOCATION: %3\r\n"
											"SERVER: %4\r\n"
											"ST: %5\r\n"
											"USN: uuid:%6\r\n"
											"HYPERHDR-FBS-PORT: %7\r\n"
											"HYPERHDR-JSS-PORT: %8\r\n"
											"HYPERHDR-NAME: %9\r\n"
											"\r\n";

SSDPServer::SSDPServer(QObject* parent)
	: QObject(parent)
	, _log(Logger::getInstance("SSDP"))
	, _udpSocket(nullptr)
	, _running(false)
{

}

SSDPServer::~SSDPServer()
{
	Debug(_log, "Prepare to shutdown");
	stop();
	Debug(_log, "SSDP server is closed");
}

void SSDPServer::initServer()
{
	Debug(_log, "Initialize the SSDP server");

	_udpSocket = new QUdpSocket(this);

	// create SERVER String
	_serverHeader = QString("%1/%2 UPnP/1.0 HyperHDR/%3")
		.arg(QSysInfo::prettyProductName(), QSysInfo::productVersion(), HYPERHDR_VERSION);

	connect(_udpSocket, &QUdpSocket::readyRead, this, &SSDPServer::readPendingDatagrams);
}

bool SSDPServer::start()
{
	Info(_log, "Starting the SSDP server");
	if (!_running && _udpSocket->bind(QHostAddress::AnyIPv4, SSDP_PORT, QAbstractSocket::ShareAddress))
	{
		_udpSocket->joinMulticastGroup(SSDP_ADDR);
		_running = true;
		return true;
	}
	return false;
}

void SSDPServer::stop()
{
	Info(_log, "Stopping the SSDP server");
	if (_running)
	{
		_udpSocket->close();
		_running = false;
	}
}

void SSDPServer::readPendingDatagrams()
{
	while (_udpSocket->hasPendingDatagrams()) {

		QByteArray datagram;
		datagram.resize(_udpSocket->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

		QString data(datagram);
		QMap<QString, QString> headers;
		// parse request
		QStringList entries = data.split('\n', Qt::SkipEmptyParts);
		for (auto entry : entries)
		{
			// http header parse skip
			if (entry.contains("HTTP/1.1"))
				continue;

			// split into key:vale, be aware that value field may contain also a ":"
			entry = entry.simplified();
			int pos = entry.indexOf(":");
			if (pos == -1)
				continue;

			headers[entry.left(pos).trimmed().toLower()] = entry.mid(pos + 1).trimmed();
		}

		// verify ssdp spec
		if (!headers.contains("man"))
			continue;

		if (headers.value("man") == "\"ssdp:discover\"")
		{
			//Debug(_log, "Received msearch from '%s:%d'. Search target: %s",QSTRING_CSTR(sender.toString()), senderPort, QSTRING_CSTR(headers.value("st")));
			emit msearchRequestReceived(headers.value("st"), headers.value("mx"), sender.toString(), senderPort);
		}
	}
}

void SSDPServer::sendMSearchResponse(const QString& st, const QString& senderIp, quint16 senderPort)
{
	QString message = UPNP_MSEARCH_RESPONSE.arg(SSDP_MAX_AGE
		, QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy HH:mm:ss GMT")
		, _descAddress
		, _serverHeader
		, st
		, _uuid
		, _fbsPort
		, _jssPort
		, _name);

	_udpSocket->writeDatagram(message.toUtf8(), QHostAddress(senderIp), senderPort);
}

void SSDPServer::sendByeBye(const QString& st)
{
	QString message = UPNP_BYEBYE_MESSAGE.arg(st, _uuid + "::" + st);

	// we repeat 3 times
	quint8 rep = 0;
	while (rep++ < 3) {
		_udpSocket->writeDatagram(message.toUtf8(), QHostAddress(SSDP_ADDR), SSDP_PORT);
	}
}

void SSDPServer::sendAlive(const QString& st)
{
	const QString tempUSN = (st == "upnp:rootdevice ") ? _uuid + "::" + st : _uuid;

	QString message = UPNP_ALIVE_MESSAGE.arg(SSDP_MAX_AGE
		, _descAddress
		, st
		, _serverHeader
		, tempUSN
		, _fbsPort
		, _jssPort
		, _name);

	// we repeat 3 times
	quint8 rep = 0;
	while (rep++ < 3) {
		_udpSocket->writeDatagram(message.toUtf8(), QHostAddress(SSDP_ADDR), SSDP_PORT);
	}
}

void SSDPServer::sendUpdate(const QString& st)
{
	QString message = UPNP_UPDATE_MESSAGE.arg(_descAddress
		, st
		, _uuid + "::" + st);

	_udpSocket->writeDatagram(message.toUtf8(), QHostAddress(SSDP_ADDR), SSDP_PORT);
}

void SSDPServer::setDescriptionAddress(const QString& addr)
{
	_descAddress = addr;
}


void SSDPServer::setUuid(const QString& uuid)
{
	_uuid = uuid;
}


void SSDPServer::setFlatBufPort(quint16 port)
{
	_fbsPort = QString::number(port);
}


quint16 SSDPServer::getFlatBufPort() const
{
	return _fbsPort.toInt();
}

void SSDPServer::setProtoBufPort(quint16 port)
{
	_pbsPort = QString::number(port);
}


quint16 SSDPServer::getProtoBufPort() const
{
	return _pbsPort.toInt();
}

void SSDPServer::setJsonServerPort(quint16 port)
{
	_jssPort = QString::number(port);
}

quint16 SSDPServer::getJsonServerPort() const
{
	return _jssPort.toInt();
}

void SSDPServer::setSSLServerPort(quint16 port)
{
	_sslPort = QString::number(port);
}

quint16 SSDPServer::getSSLServerPort() const
{
	return _sslPort.toInt();
}

void SSDPServer::setWebServerPort(quint16 port)
{
	_webPort = QString::number(port);
}

quint16 SSDPServer::getWebServerPort() const
{
	return _webPort.toInt();
}


void SSDPServer::setHyperhdrName(const QString& name)
{
	_name = name;
}


QString SSDPServer::getHyperhdrName() const
{
	return _name;
}
