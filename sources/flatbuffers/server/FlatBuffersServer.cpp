#include "HyperhdrConfig.h"
#include <flatbuffers/parser/FlatBuffersParser.h>
#include <flatbuffers/server/FlatBuffersServerConnection.h>
#include <flatbuffers/server/FlatBuffersServer.h>

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

FlatBuffersServer::FlatBuffersServer(std::shared_ptr<NetOrigin> netOrigin, const QJsonDocument& config, const QString& configurationPath, QObject* parent)
	: QObject(parent)
	, _server(new QTcpServer(this))
	, _domain(new QLocalServer(this))
	, _netOrigin(netOrigin)
	, _log(Logger::getInstance("FLATBUFSERVER"))
	, _timeout(5000)
	, _port(19400)
	, _config(config)
	, _configurationPath(configurationPath)
	, _userLutFile("")
	, _currentLutPixelFormat(PixelFormat::RGB24)
	, _flatbufferToneMappingMode(0)
	, _quarterOfFrameMode(false)
{
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetLut, this, &FlatBuffersServer::signalSetLutHandler, Qt::BlockingQueuedConnection);
}

FlatBuffersServer::~FlatBuffersServer()
{
	Debug(_log, "Prepare to shutdown");

	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetLut, this, &FlatBuffersServer::signalSetLutHandler);
	stopServer();

	Debug(_log, "FlatBuffersServer instance is closed");
}

void FlatBuffersServer::initServer()
{
	if (_server != nullptr)
		connect(_server, &QTcpServer::newConnection, this, &FlatBuffersServer::handlerNewConnection);
	if (_domain != nullptr)
		connect(_domain, &QLocalServer::newConnection, this, &FlatBuffersServer::handlerNewConnection);

	connect(this, &FlatBuffersServer::SignalImportFromProto, this, &FlatBuffersServer::handlerImportFromProto);

	// apply config
	handleSettingsUpdate(settings::type::FLATBUFSERVER, _config);

	loadLutFile();
}

void FlatBuffersServer::signalRequestSourceHandler(hyperhdr::Components component, int instanceIndex, bool listen)
{
	if (component == hyperhdr::Components::COMP_HDR)
	{
		if (instanceIndex < 0)
		{
			_hdrToneMappingEnabled = (listen) ? _flatbufferToneMappingMode : 0;

			Info(_log, "Tone mapping: %i", _hdrToneMappingEnabled);

			if (_hdrToneMappingEnabled || _currentLutPixelFormat == PixelFormat::YUYV)
				loadLutFile();
			else
			{
				_lutBufferInit = false;
				_lut.resize(0);
			}

		}
		emit SignalSetNewComponentStateToAllInstances(hyperhdr::Components::COMP_HDR, getHdrToneMappingEnabled());
	}
}

int FlatBuffersServer::getHdrToneMappingEnabled()
{
	return ((_hdrToneMappingEnabled != 0) && _lutBufferInit) ? 1 : 0;
}

void FlatBuffersServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
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
		_flatbufferToneMappingMode = obj["hdrToneMapping"].toBool(false) ? 1 : 0;

		signalRequestSourceHandler(hyperhdr::Components::COMP_HDR, -1, _flatbufferToneMappingMode);

		// new timeout just for new connections
		_timeout = obj["timeout"].toInt(5000);
		// enable check
		obj["enable"].toBool(true) ? startServer() : stopServer();

		_quarterOfFrameMode = obj["quarterOfFrameMode"].toBool(false);

		Info(_log, "Tone mapping: %i", _flatbufferToneMappingMode);
		Info(_log, "NV12 quarter of frame mode: %i", _quarterOfFrameMode);		
	}
}

void FlatBuffersServer::setupClient(FlatBuffersServerConnection* client)
{
	connect(client, &FlatBuffersServerConnection::SignalClientDisconnected, this, &FlatBuffersServer::handlerClientDisconnected);
	connect(client, &FlatBuffersServerConnection::SignalClearGlobalInput, GlobalSignals::getInstance(), &GlobalSignals::SignalClearGlobalInput);
	connect(client, &FlatBuffersServerConnection::SignalDirectImageReceivedInTempBuffer, this, &FlatBuffersServer::handlerImageReceived, Qt::DirectConnection);
	connect(client, &FlatBuffersServerConnection::SignalSetGlobalColor, GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalColor);
	_openConnections.append(client);
}

void FlatBuffersServer::handlerNewConnection()
{
	while (_server != nullptr && _server->hasPendingConnections())
	{
		if (QTcpSocket* socket = _server->nextPendingConnection())
		{
			if (_netOrigin->accessAllowed(socket->peerAddress(), socket->localAddress()))
			{
				Debug(_log, "New connection from %s", QSTRING_CSTR(socket->peerAddress().toString()));
				FlatBuffersServerConnection* client = new FlatBuffersServerConnection(socket, nullptr, _timeout, this);
				QString anyError = client->getErrorString();
				if (anyError.isEmpty())
				{
					// internal
					setupClient(client);
				}
				else
				{
					Error(_log, "Could not initialize server client: %s", QSTRING_CSTR(anyError));
					client->deleteLater();
				}
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
			FlatBuffersServerConnection* client = new FlatBuffersServerConnection(nullptr, socket, _timeout, this);
			QString anyError = client->getErrorString();
			if (anyError.isEmpty())
			{				
				// internal
				setupClient(client);
			}
			else
			{
				Error(_log, "Could not initialize server client: %s", QSTRING_CSTR(anyError));
				client->deleteLater();
			}
		}
	}
}

void FlatBuffersServer::handlerClientDisconnected(FlatBuffersServerConnection* client)
{
	if (client != nullptr)
	{
		client->deleteLater();
		_openConnections.removeAll(client);
	}
}

void FlatBuffersServer::startServer()
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

void FlatBuffersServer::stopServer()
{
	if ((_server != nullptr &&_server->isListening()) || (_domain != nullptr && _domain->isListening()))
	{
		QVectorIterator<FlatBuffersServerConnection*> i(_openConnections);
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

QString FlatBuffersServer::GetSharedLut()
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

void FlatBuffersServer::loadLutFile()
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

	LutLoader::loadLutFile(_log, _currentLutPixelFormat, files);
}

void FlatBuffersServer::handlerImportFromProto(int priority, int duration, const Image<ColorRgb>& image, QString clientDescription)
{
	if (_currentLutPixelFormat != PixelFormat::RGB24 && _hdrToneMappingEnabled)
	{
		_currentLutPixelFormat = PixelFormat::RGB24;
		loadLutFile();
	}

	if (getHdrToneMappingEnabled())
		FrameDecoder::applyLUT((uint8_t*)image.rawMem(), image.width(), image.height(), _lut.data(), getHdrToneMappingEnabled());

	emit GlobalSignals::getInstance()->SignalSetGlobalImage(priority, image, duration, hyperhdr::Components::COMP_PROTOSERVER, clientDescription);
}

void FlatBuffersServer::handlerImageReceived(int priority, FlatBuffersParser::FlatbuffersTransientImage* flatImage, int timeout_ms, hyperhdr::Components origin, QString clientDescription)
{
	if (QThread::currentThread() != this->thread())
	{
		Error(_log, "Sanity check. FlatBuffersServer::handlerImageReceived uses the wrong thread affiliation.");
		return;
	}

	if (flatImage->format == FlatBuffersParser::FLATBUFFERS_IMAGE_FORMAT::RGB)
	{
		if (_currentLutPixelFormat != PixelFormat::RGB24)
		{
			_currentLutPixelFormat = PixelFormat::RGB24;						
			if (_hdrToneMappingEnabled)
			{
				loadLutFile();
			}

			Debug(_log, "Received first RGB frame. Image size: %i (%i x %i)", flatImage->size, flatImage->width, flatImage->height);
		}

		if (flatImage->size != flatImage->width * flatImage->height * 3 || flatImage->size == 0)
		{
			Error(_log, "The RGB image data size does not match the width and height or it's empty. Image size: %i (%i x %i)", flatImage->size, flatImage->width, flatImage->height);
		}		
		else
		{
			Image<ColorRgb> image(flatImage->width, flatImage->height);
			memmove(image.rawMem(), flatImage->firstPlane.data, flatImage->size);

			if (getHdrToneMappingEnabled())
				FrameDecoder::applyLUT((uint8_t*)image.rawMem(), image.width(), image.height(), _lut.data(), getHdrToneMappingEnabled());

			emit GlobalSignals::getInstance()->SignalSetGlobalImage(priority, image, timeout_ms, origin, clientDescription);
		}
	}
	else if (flatImage->format == FlatBuffersParser::FLATBUFFERS_IMAGE_FORMAT::NV12)
	{
		if (_currentLutPixelFormat != PixelFormat::YUYV)
		{
			_currentLutPixelFormat = PixelFormat::YUYV;
			loadLutFile();

			Debug(_log, "Received first NV12 frame. First plane size: %i (stride: %i). Second plane size: %i (stride: %i). Image size: %i (%i x %i)",
				flatImage->firstPlane.size, flatImage->firstPlane.stride,
				flatImage->secondPlane.size, flatImage->secondPlane.stride,
				flatImage->size, flatImage->width, flatImage->height);
		}

		if (!_lutBufferInit)
		{
			Error(_log, "The LUT file is not loaded");
		}
		else if (flatImage->size != ((flatImage->width * flatImage->height * 3) / 2) || flatImage->size == 0)
		{
			Error(_log, "The NV12 image data size (%i) does not match the width and height (%i) or it's empty", flatImage->size, ((flatImage->width * flatImage->height * 3) / 2));
		}
		else if ((flatImage->firstPlane.stride != flatImage->secondPlane.stride) ||
			(flatImage->firstPlane.stride != 0 && flatImage->firstPlane.stride != flatImage->width))
		{
			Error(_log, "The NV12 image data contains incorrect stride size: %i and %i, expected %i or 0",
						flatImage->firstPlane.stride, flatImage->secondPlane.stride, flatImage->width);
		}
		else
		{
			Image<ColorRgb> image(flatImage->width, flatImage->height);

			if (_quarterOfFrameMode)
			{
				FrameDecoder::processQImage(					
					flatImage->firstPlane.data, flatImage->secondPlane.data, flatImage->width, flatImage->height, flatImage->width, PixelFormat::NV12, _lut.data(), image);
			}
			else
			{
				FrameDecoder::processImage(
					0, 0, 0, 0,
					flatImage->firstPlane.data, flatImage->secondPlane.data, flatImage->width, flatImage->height, flatImage->width, PixelFormat::NV12, _lut.data(), image);
			}
			emit GlobalSignals::getInstance()->SignalSetGlobalImage(priority, image, timeout_ms, origin, clientDescription);
		}
	}
	else
	{
		Error(_log, "Unsupported flatbuffers image format");
	}
}

void FlatBuffersServer::signalSetLutHandler(MemoryBuffer<uint8_t>* lut)
{
	if (lut != nullptr && _lut.size() >= lut->size())
	{
		memcpy(_lut.data(), lut->data(), lut->size());
		Info(_log, "The byte array loaded into LUT");
	}
	else
		Error(_log, "Could not set LUT: current size = %i, incoming size = %i", _lut.size(), (lut != nullptr) ? lut->size() : 0);
}
