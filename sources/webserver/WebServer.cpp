#include <QTcpSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QFileInfo>
#include <QJsonObject>
#include <QTcpServer>
#include <QDateTime>

#include "QtHttpRequest.h"
#include "webserver/WebServer.h"
#include "HyperhdrConfig.h"
#include "FileServer.h"
#include "QtHttpServer.h"

#ifdef USE_STATIC_QT_PLUGINS
	#include <QtPlugin>
	Q_IMPORT_PLUGIN(QTlsBackendOpenSSLPlugin)
#endif

// bonjour
#ifdef ENABLE_BONJOUR
	#include <bonjour/BonjourServiceRegister.h>
#endif

// netUtil
#include <utils/NetOrigin.h>

std::weak_ptr<FileServer> WebServer::globalStaticFileServing;

WebServer::WebServer(std::shared_ptr<NetOrigin> netOrigin, const QJsonDocument& config, bool useSsl, QObject* parent)
	: QObject(parent)
	, _port(0)
	, _config(config)
	, _useSsl(useSsl)
	, _log(Logger::getInstance("WEBSERVER"))
	, _server(nullptr)
	, _netOrigin(netOrigin)
{

}

WebServer::~WebServer()
{
	Debug(_log, "Prepare to shutdown");
	stop();
	Debug(_log, "Webserver instance is closed");
}

void WebServer::start()
{
	if (_server != nullptr)
		_server->start(_port);
}

void WebServer::stop()
{
	if (_server != nullptr)
		_server->stop();
}

void WebServer::initServer()
{
	Info(_log, "Initialize Webserver");
	_server = new QtHttpServer(_netOrigin, this);
	_server->setServerName(QString("HyperHDR WebServer %1").arg((!_useSsl) ? "(HTTP)" : "(HTTPS)"));

	if (_useSsl)
	{
		_server->setUseSecure();
		WEBSERVER_DEFAULT_PORT = 8092;
	}

	connect(_server, &QtHttpServer::started, this, &WebServer::onServerStarted);
	connect(_server, &QtHttpServer::stopped, this, &WebServer::onServerStopped);
	connect(_server, &QtHttpServer::error, this, &WebServer::onServerError);

	// create StaticFileServing
	connect(_server, &QtHttpServer::requestNeedsReply, this, &WebServer::onInitRequestNeedsReply);

	// init
	handleSettingsUpdate(settings::type::WEBSERVER, _config);
}

void WebServer::onInitRequestNeedsReply(QtHttpRequest* request, QtHttpReply* reply)
{
	if (_staticFileServing == nullptr)
	{
		_staticFileServing = globalStaticFileServing.lock();
		if (_staticFileServing == nullptr)
		{
			_staticFileServing = std::make_shared<FileServer>();
			globalStaticFileServing = _staticFileServing;
		}
	}

	_staticFileServing->onRequestNeedsReply(request, reply);
}

void WebServer::onServerStarted(quint16 port)
{
	Info(_log, "Started: '%s' on port: %d", QSTRING_CSTR(_server->getServerName()), port);

#ifdef ENABLE_BONJOUR
	if (!_useSsl)
	{
		if (_serviceRegister == nullptr)
		{
			_serviceRegister = new BonjourServiceRegister(this, DiscoveryRecord::Service::HyperHDR, port);
			_serviceRegister->registerService();
		}
		else if (_serviceRegister->getPort() != port)
		{
			delete _serviceRegister;
			_serviceRegister = new BonjourServiceRegister(this, DiscoveryRecord::Service::HyperHDR, port);
			_serviceRegister->registerService();
		}
	}
#endif

	emit stateChange(true);
}

void WebServer::handleSettingsUpdate(settings::type type, QJsonDocument config)
{
	if (type == settings::type::WEBSERVER)
	{
		Info(_log, "Apply Webserver settings");
		QJsonObject obj = config.object();

		_baseUrl = obj["document_root"].toString(WEBSERVER_DEFAULT_PATH);


		if ((_baseUrl != ":/www") && !_baseUrl.trimmed().isEmpty())
		{
			QFileInfo info(_baseUrl);
			if (!info.exists() || !info.isDir())
			{
				Error(_log, "Document_root '%s' is invalid", QSTRING_CSTR(_baseUrl));
				_baseUrl = WEBSERVER_DEFAULT_PATH;
			}
		}
		else
			_baseUrl = WEBSERVER_DEFAULT_PATH;

		Info(_log, "Set document root to: %s", QSTRING_CSTR(_baseUrl));
		FileServer::setBaseUrl(_baseUrl);

		// ssl different port
		quint16 newPort = _useSsl ? obj["sslPort"].toInt(WEBSERVER_DEFAULT_PORT) : obj["port"].toInt(WEBSERVER_DEFAULT_PORT);
		if (_port != newPort)
		{
			_port = newPort;
			stop();
		}

		// eval if the port is available, will be incremented if not
		if (!_server->isListening())
			portAvailable(_port, _log);

		// on ssl we want .key .cert and probably key password
		if (_useSsl)
		{
			QString keyPath = obj["keyPath"].toString(WEBSERVER_DEFAULT_KEY_PATH);
			QString crtPath = obj["crtPath"].toString(WEBSERVER_DEFAULT_CRT_PATH);

			QSslKey currKey = _server->getPrivateKey();
			QList<QSslCertificate> currCerts = _server->getCertificates();

			// check keyPath
			if ((keyPath != WEBSERVER_DEFAULT_KEY_PATH) && !keyPath.trimmed().isEmpty())
			{
				QFileInfo kinfo(keyPath);
				if (!kinfo.exists())
				{
					Error(_log, "No SSL key found at '%s' falling back to internal", QSTRING_CSTR(keyPath));
					keyPath = WEBSERVER_DEFAULT_KEY_PATH;
				}
			}
			else
				keyPath = WEBSERVER_DEFAULT_KEY_PATH;

			// check crtPath
			if ((crtPath != WEBSERVER_DEFAULT_CRT_PATH) && !crtPath.trimmed().isEmpty())
			{
				QFileInfo cinfo(crtPath);
				if (!cinfo.exists())
				{
					Error(_log, "No SSL certificate found at '%s' falling back to internal", QSTRING_CSTR(crtPath));
					crtPath = WEBSERVER_DEFAULT_CRT_PATH;
				}
			}
			else
				crtPath = WEBSERVER_DEFAULT_CRT_PATH;

			// load and verify crt
			QFile cfile(crtPath);
			cfile.open(QIODevice::ReadOnly);
			QList<QSslCertificate> validList;
			QList<QSslCertificate> cList = QSslCertificate::fromDevice(&cfile, QSsl::Pem);
			cfile.close();

			// Filter for valid certs
			for (const auto& entry : cList)
			{
				if (!entry.isNull() && QDateTime::currentDateTime().daysTo(entry.expiryDate()) > 0)
					validList.append(entry);
				else
					Error(_log, "The provided SSL certificate is invalid/not supported/reached expiry date ('%s')", QSTRING_CSTR(crtPath));
			}

			if (!validList.isEmpty())
			{
				Info(_log, "Setup SSL certificate");
				_server->setCertificates(validList);
			}
			else {
				Error(_log, "No valid SSL certificate has been found ('%s'). Did you install OpenSSL?", QSTRING_CSTR(crtPath));
			}

			// load and verify key
			QFile kfile(keyPath);
			kfile.open(QIODevice::ReadOnly);
			// The key should be RSA enrcrypted and PEM format, optional the passPhrase
			QSslKey key(&kfile, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, obj["keyPassPhrase"].toString().toUtf8());
			kfile.close();

			if (key.isNull())
			{
				Error(_log, "The provided SSL key is invalid or not supported use RSA encrypt and PEM format ('%s')", QSTRING_CSTR(keyPath));
			}
			else
			{
				Info(_log, "Setup private SSL key");
				_server->setPrivateKey(key);
			}
		}

		start();
		emit portChanged(_port);
	}
}

bool WebServer::portAvailable(quint16& port, Logger* log)
{
	const quint16 prevPort = port;
	QTcpServer server;
	while (!server.listen(QHostAddress::Any, port))
	{
		Warning(log, "Port '%d' is already in use, will increment", port);
		port++;
	}
	server.close();
	if (port != prevPort)
	{
		Warning(log, "The requested Port '%d' was already in use, will use Port '%d' instead", prevPort, port);
		return false;
	}
	return true;
}

void WebServer::setSsdpXmlDesc(const QString& desc)
{
	FileServer::setSsdpXmlDesc(desc);
}

void WebServer::onServerError(QString msg)
{
	Error(_log, "%s", QSTRING_CSTR(msg));
}

void WebServer::onServerStopped()
{
	Info(_log, "Stopped: %s", QSTRING_CSTR(_server->getServerName()));
	emit stateChange(false);
}

