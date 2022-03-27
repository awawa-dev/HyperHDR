#include <flatbufserver/FlatBufferServer.h>
#include "FlatBufferClient.h"
#include "HyperhdrConfig.h"

// util
#include <utils/NetOrigin.h>
#include <utils/GlobalSignals.h>
#include <base/HyperHdrIManager.h>

// bonjour
#ifdef ENABLE_AVAHI
#include <bonjour/bonjourserviceregister.h>
#endif

// qt
#include <QJsonObject>
#include <QTcpServer>
#include <QLocalServer>
#include <QTcpSocket>
#include <QFile>
#include <QCoreApplication>
#include <QFileInfo>

#define LUT_FILE_SIZE 50331648

FlatBufferServer* FlatBufferServer::instance = nullptr;

FlatBufferServer::FlatBufferServer(const QJsonDocument& config, const QString& configurationPath, QObject* parent)
	: QObject(parent)
	, _server(new QTcpServer(this))
	, _domain(new QLocalServer(this))
	, _log(Logger::getInstance("FLATBUFSERVER"))
	, _timeout(5000)
	, _config(config)
	, _hdrToneMappingMode(false)
	, _lutBuffer(nullptr)
	, _lutBufferInit(false)
	, _configurationPath(configurationPath)
{
	FlatBufferServer::instance = this;
}

FlatBufferServer::~FlatBufferServer()
{
	stopServer();

	delete _server;
	delete _domain;

	if (_lutBuffer != NULL)
		free(_lutBuffer);
	_lutBuffer = NULL;

	FlatBufferServer::instance = nullptr;
}

void FlatBufferServer::initServer()
{
	_netOrigin = NetOrigin::getInstance();

	if (_server != nullptr)
		connect(_server, &QTcpServer::newConnection, this, &FlatBufferServer::newConnection);
	if (_domain != nullptr)
		connect(_domain, &QLocalServer::newConnection, this, &FlatBufferServer::newConnection);

	// apply config
	handleSettingsUpdate(settings::type::FLATBUFSERVER, _config);

	loadLutFile();
}

void FlatBufferServer::setHdrToneMappingEnabled(int mode)
{
	bool status = _hdrToneMappingMode && mode;

	if (status)
		loadLutFile();

	// inform clients
	emit hdrToneMappingChanged((_lutBufferInit && status)?mode :0, _lutBuffer);
}

void FlatBufferServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::FLATBUFSERVER)
	{
		const QJsonObject& obj = config.object();

		quint16 port = obj["port"].toInt(19400);

		// port check
		if (_server != nullptr && _server->serverPort() != port)
		{
			stopServer();
			_port = port;
		}

		// HDR tone mapping
		_hdrToneMappingMode = obj["hdrToneMapping"].toBool(false) ? obj["hdrToneMappingMode"].toInt(1) : 0;

		setHdrToneMappingEnabled(_hdrToneMappingMode);

#if !defined(ENABLE_MF) && !defined(ENABLE_AVF) && !defined(ENABLE_V4L2)
		emit HyperHdrIManager::getInstance()->setNewComponentStateToAllInstances(hyperhdr::Components::COMP_HDR, (_hdrToneMappingMode != 0));
#endif

		// new timeout just for new connections
		_timeout = obj["timeout"].toInt(5000);
		// enable check
		obj["enable"].toBool(true) ? startServer() : stopServer();
	}
}

void FlatBufferServer::setupClient(FlatBufferClient* client)
{
	connect(client, &FlatBufferClient::clientDisconnected, this, &FlatBufferServer::clientDisconnected);
	connect(client, &FlatBufferClient::registerGlobalInput, GlobalSignals::getInstance(), &GlobalSignals::registerGlobalInput);
	connect(client, &FlatBufferClient::clearGlobalInput, GlobalSignals::getInstance(), &GlobalSignals::clearGlobalInput);
	connect(client, &FlatBufferClient::setGlobalInputImage, GlobalSignals::getInstance(), &GlobalSignals::setGlobalImage);
	connect(client, &FlatBufferClient::setGlobalInputColor, GlobalSignals::getInstance(), &GlobalSignals::setGlobalColor);
	connect(GlobalSignals::getInstance(), &GlobalSignals::globalRegRequired, client, &FlatBufferClient::registationRequired);
	connect(this, &FlatBufferServer::hdrToneMappingChanged, client, &FlatBufferClient::setHdrToneMappingEnabled);
	_openConnections.append(client);
}

void FlatBufferServer::newConnection()
{
	while (_server != nullptr && _server->hasPendingConnections())
	{
		if (QTcpSocket* socket = _server->nextPendingConnection())
		{
			if (_netOrigin->accessAllowed(socket->peerAddress(), socket->localAddress()))
			{
				Debug(_log, "New connection from %s", QSTRING_CSTR(socket->peerAddress().toString()));
				FlatBufferClient* client = new FlatBufferClient(socket, nullptr, _timeout, _hdrToneMappingMode, _lutBuffer, this);
				// internal
				setupClient(client);
			}
			else
				socket->close();
		}
	}
	while (_domain != nullptr && _domain->hasPendingConnections())
	{
		if (QLocalSocket* socket = _domain->nextPendingConnection())
		{			
			Debug(_log, "New local domain connection");
			FlatBufferClient* client = new FlatBufferClient(nullptr, socket, _timeout, _hdrToneMappingMode, _lutBuffer, this);
			// internal
			setupClient(client);
		}
	}
}

void FlatBufferServer::clientDisconnected()
{
	FlatBufferClient* client = qobject_cast<FlatBufferClient*>(sender());
	client->deleteLater();
	_openConnections.removeAll(client);
}

void FlatBufferServer::startServer()
{
	if (_server != nullptr && !_server->isListening())
	{
		if (!_server->listen(QHostAddress::Any, _port))
		{
			Error(_log, "Failed to bind port %d", _port);
		}
		else
		{
			Info(_log, "Started on port %d", _port);
#ifdef ENABLE_AVAHI
			if (_serviceRegister == nullptr)
			{
				_serviceRegister = new BonjourServiceRegister(this);
				_serviceRegister->registerService("_hyperhdr-flatbuf._tcp", _port);
			}
			else if (_serviceRegister->getPort() != _port)
			{
				delete _serviceRegister;
				_serviceRegister = new BonjourServiceRegister(this);
				_serviceRegister->registerService("_hyperhdr-flatbuf._tcp", _port);
			}
#endif
		}
	}
	if (_domain != nullptr && !_domain->isListening())
	{
		if (!_domain->listen(HYPERHDR_DOMAIN_SERVER))
			Error(_log, "Could not start local domain socket server '%s'", QSTRING_CSTR(QString(HYPERHDR_DOMAIN_SERVER)));
		else
			Info(_log, "Started local domain socket server: '%s'", QSTRING_CSTR(_domain->serverName()));
	}
}

void FlatBufferServer::stopServer()
{
	if ((_server != nullptr &&_server->isListening()) || (_domain != nullptr && _domain->isListening()))
	{
		// close client connections
		for (const auto& client : _openConnections)
		{
			client->forceClose();
		}

		// closing
		if (_server != nullptr)
			_server->close();

		if (_domain != nullptr)
			_domain->close();

		Info(_log, "Stopped");
	}
}

QString FlatBufferServer::GetSharedLut()
{
#ifdef __APPLE__
	QString ret = QString("%1%2").arg(QCoreApplication::applicationDirPath()).arg("/../lut");
	QFileInfo info(ret);
	ret = info.absoluteFilePath();
	return ret;
#else
	return QCoreApplication::applicationDirPath();
#endif
}

// copied from Grabber::loadLutFile()
// color should always be RGB24 for flatbuffers
void FlatBufferServer::loadLutFile()
{
	QString fileName01 = QString("%1%2").arg(_configurationPath).arg("/flat_lut_lin_tables.3d");
	QString fileName02 = QString("%1%2").arg(GetSharedLut()).arg("/flat_lut_lin_tables.3d");
	QString fileName1 = QString("%1%2").arg(_configurationPath).arg("/lut_lin_tables.3d");
	QString fileName2 = QString("%1%2").arg(GetSharedLut()).arg("/lut_lin_tables.3d");
	QList<QString> files({ fileName01, fileName02, fileName1, fileName2 });

#ifdef __linux__
	QString fileName03 = QString("/usr/share/hyperhdr/lut/flat_lut_lin_tables.3d");
	QString fileName3 = QString("/usr/share/hyperhdr/lut/lut_lin_tables.3d");
	files.append(fileName03);
	files.append(fileName3);
#endif

	_lutBufferInit = false;

	if (_hdrToneMappingMode)
	{
		for (QString fileName3d : files)
		{
			QFile file(fileName3d);

			if (file.open(QIODevice::ReadOnly))
			{
				size_t length;
				Debug(_log, "LUT file found: %s", QSTRING_CSTR(fileName3d));

				length = file.size();

				if (length == LUT_FILE_SIZE * 3 || length == LUT_FILE_SIZE)
				{
					qint64 index = 0; // RGB24

					file.seek(index);

					if (_lutBuffer == NULL)
						_lutBuffer = (unsigned char*)malloc(length + 4);

					if (file.read((char*)_lutBuffer, LUT_FILE_SIZE) != LUT_FILE_SIZE)
					{
						Error(_log, "Error reading LUT file %s", QSTRING_CSTR(fileName3d));
					}
					else
					{
						_lutBufferInit = true;
						Info(_log, "Found and loaded LUT: '%s'", QSTRING_CSTR(fileName3d));
					}
				}
				else
					Error(_log, "LUT file has invalid length: %i %s. Please generate new one LUT table using the generator page.", length, QSTRING_CSTR(fileName3d));

				file.close();

				return;
			}
			else
				Warning(_log, "LUT file is not found here: %s", QSTRING_CSTR(fileName3d));
		}

		Error(_log, "Could not find any required LUT file");
	}
}
