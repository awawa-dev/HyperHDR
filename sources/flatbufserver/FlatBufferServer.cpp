#include <flatbufserver/FlatBufferServer.h>
#include "FlatBufferClient.h"
#include "HyperhdrConfig.h"

// util
#include <utils/NetOrigin.h>
#include <utils/GlobalSignals.h>
#include <base/HyperHdrManager.h>
#include <utils/FrameDecoder.h>

// qt
#include <QJsonObject>
#include <QTcpServer>
#include <QLocalServer>
#include <QTcpSocket>
#include <QFile>
#include <QCoreApplication>
#include <QFileInfo>

#define LUT_FILE_SIZE 50331648

FlatBufferServer::FlatBufferServer(std::shared_ptr<NetOrigin> netOrigin, const QJsonDocument& config, const QString& configurationPath, QObject* parent)
	: QObject(parent)
	, _server(new QTcpServer(this))
	, _domain(new QLocalServer(this))
	, _netOrigin(netOrigin)
	, _log(Logger::getInstance("FLATBUFSERVER"))
	, _timeout(5000)
	, _port(19400)
	, _config(config)
	, _hdrToneMappingMode(0)
	, _realHdrToneMappingMode(0)
	, _lutBufferInit(false)
	, _configurationPath(configurationPath)
	, _userLutFile("")
{	
}

FlatBufferServer::~FlatBufferServer()
{
	Debug(_log, "Prepare to shutdown");

	stopServer();

	Debug(_log, "FlatBufferServer instance is closed");
}

void FlatBufferServer::initServer()
{
	if (_server != nullptr)
		connect(_server, &QTcpServer::newConnection, this, &FlatBufferServer::handlerNewConnection);
	if (_domain != nullptr)
		connect(_domain, &QLocalServer::newConnection, this, &FlatBufferServer::handlerNewConnection);

	connect(this, &FlatBufferServer::SignalImportFromProto, this, &FlatBufferServer::handlerImportFromProto);

	// apply config
	handleSettingsUpdate(settings::type::FLATBUFSERVER, _config);

	loadLutFile();
}

void FlatBufferServer::signalRequestSourceHandler(hyperhdr::Components component, int instanceIndex, bool listen)
{
	if (component == hyperhdr::Components::COMP_HDR)
	{
		if (instanceIndex < 0)
		{
			bool status = (_hdrToneMappingMode != 0) && listen;

			if (status)
				loadLutFile();

			_realHdrToneMappingMode = (_lutBufferInit && status) ? listen : 0;
		}
		emit SignalSetNewComponentStateToAllInstances(hyperhdr::Components::COMP_HDR, (_realHdrToneMappingMode != 0));
	}
}

int FlatBufferServer::getHdrToneMappingEnabled()
{
	return _realHdrToneMappingMode;
}

void FlatBufferServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::FLATBUFSERVER)
	{
		const QJsonObject& obj = config.object();

		if (obj.keys().contains(BASEAPI_FLATBUFFER_USER_LUT_FILE))
		{
			QString filename = obj[BASEAPI_FLATBUFFER_USER_LUT_FILE].toString();
			_userLutFile = filename.replace("~", "").replace("/", "").replace("\\", "").replace("..", "");
			Info(_log, "Setting user LUT filename to: '%s'", QSTRING_CSTR(_userLutFile));
			return;
		}


		quint16 port = obj["port"].toInt(19400);

		// port check
		if (_server != nullptr && _server->serverPort() != port)
		{
			stopServer();
			_port = port;
		}

		// HDR tone mapping
		_hdrToneMappingMode = obj["hdrToneMapping"].toBool(false) ? obj["hdrToneMappingMode"].toInt(1) : 0;

		signalRequestSourceHandler(hyperhdr::Components::COMP_HDR, -1, _hdrToneMappingMode);

		// new timeout just for new connections
		_timeout = obj["timeout"].toInt(5000);
		// enable check
		obj["enable"].toBool(true) ? startServer() : stopServer();
	}
}

void FlatBufferServer::setupClient(FlatBufferClient* client)
{
	connect(client, &FlatBufferClient::SignalClientDisconnected, this, &FlatBufferServer::handlerClientDisconnected);
	connect(client, &FlatBufferClient::SignalClearGlobalInput, GlobalSignals::getInstance(), &GlobalSignals::SignalClearGlobalInput);
	connect(client, &FlatBufferClient::SignalImageReceived, this, &FlatBufferServer::handlerImageReceived);
	connect(client, &FlatBufferClient::SignalSetGlobalColor, GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalColor);
	_openConnections.append(client);
}

void FlatBufferServer::handlerNewConnection()
{
	while (_server != nullptr && _server->hasPendingConnections())
	{
		if (QTcpSocket* socket = _server->nextPendingConnection())
		{
			if (_netOrigin->accessAllowed(socket->peerAddress(), socket->localAddress()))
			{
				Debug(_log, "New connection from %s", QSTRING_CSTR(socket->peerAddress().toString()));
				FlatBufferClient* client = new FlatBufferClient(socket, nullptr, _timeout, this);
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
			FlatBufferClient* client = new FlatBufferClient(nullptr, socket, _timeout, this);
			// internal
			setupClient(client);
		}
	}
}

void FlatBufferServer::handlerClientDisconnected(FlatBufferClient* client)
{
	if (client != nullptr)
	{
		client->deleteLater();
		_openConnections.removeAll(client);
	}
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
		QVectorIterator<FlatBufferClient*> i(_openConnections);
		while (i.hasNext())
		{
			const auto& client = i.next();
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

	if (!_userLutFile.isEmpty())
	{
		QString userFile = QString("%1/%2").arg(_configurationPath).arg(_userLutFile);
		files.prepend(userFile);
		Debug(_log, "Adding user LUT file for searching: %s", QSTRING_CSTR(userFile));
	}

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

					_lut.resize(LUT_FILE_SIZE + 64);

					if (file.read((char*)_lut.data(), LUT_FILE_SIZE) != LUT_FILE_SIZE)
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

void FlatBufferServer::handlerImportFromProto(int priority, int duration, const Image<ColorRgb>& image, QString clientDescription)
{
	if (getHdrToneMappingEnabled())
		FrameDecoder::applyLUT((uint8_t*)image.rawMem(), image.width(), image.height(), _lut.data(), getHdrToneMappingEnabled());

	emit GlobalSignals::getInstance()->SignalSetGlobalImage(priority, image, duration, hyperhdr::Components::COMP_PROTOSERVER, clientDescription);
}

void FlatBufferServer::handlerImageReceived(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin, QString clientDescription)
{
	if (getHdrToneMappingEnabled())
		FrameDecoder::applyLUT((uint8_t*)image.rawMem(), image.width(), image.height(), _lut.data(), getHdrToneMappingEnabled());
	
	emit GlobalSignals::getInstance()->SignalSetGlobalImage(priority, image, timeout_ms, origin, clientDescription);
}

