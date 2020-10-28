#include <cassert>
#include <stdlib.h>

#include <QCoreApplication>
#include <QResource>
#include <QLocale>
#include <QFile>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <cstdint>
#include <limits>
#include <QThread>

#include <utils/Components.h>
#include <utils/JsonUtils.h>
#include <utils/Image.h>

#include <HyperionConfig.h> // Required to determine the cmake options

// bonjour browser
#ifdef ENABLE_AVAHI
#include <bonjour/bonjourbrowserwrapper.h>
#endif
#include <jsonserver/JsonServer.h>
#include <webserver/WebServer.h>
#include "hyperiond.h"

// Flatbuffer Server
#include <flatbufserver/FlatBufferServer.h>

// Protobuffer Server
#include <protoserver/ProtoServer.h>

// ssdp
#include <ssdp/SSDPHandler.h>

// settings
#include <hyperion/SettingsManager.h>

// AuthManager
#include <hyperion/AuthManager.h>

// InstanceManager Hyperion
#include <hyperion/HyperionIManager.h>

// NetOrigin checks
#include <utils/NetOrigin.h>

// Init Python
#include <python/PythonInit.h>

// EffectFileHandler
#include <effectengine/EffectFileHandler.h>

HyperionDaemon *HyperionDaemon::daemon = nullptr;

HyperionDaemon::HyperionDaemon(const QString rootPath, QObject *parent, bool logLvlOverwrite)
		: QObject(parent), _log(Logger::getInstance("DAEMON"))
		, _instanceManager(new HyperionIManager(rootPath, this))
		, _authManager(new AuthManager(this))
#ifdef ENABLE_AVAHI
		, _bonjourBrowserWrapper(new BonjourBrowserWrapper())
#endif
		, _netOrigin(new NetOrigin(this))
		, _pyInit(new PythonInit())
		, _webserver(nullptr)
		, _sslWebserver(nullptr)
		, _jsonServer(nullptr)
		, _v4l2Grabber(nullptr)
		, _qtcGrabber(nullptr)
		, _ssdp(nullptr)
		, _currVideoMode(VideoMode::VIDEO_2D)
		, _rootPath(rootPath)
{
	HyperionDaemon::daemon = this;

	// Register metas for thread queued connection
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");
	qRegisterMetaType<hyperion::Components>("hyperion::Components");
	qRegisterMetaType<settings::type>("settings::type");
	qRegisterMetaType<VideoMode>("VideoMode");
	qRegisterMetaType<QMap<quint8, QJsonObject>>("QMap<quint8,QJsonObject>");
	qRegisterMetaType<std::vector<ColorRgb>>("std::vector<ColorRgb>");

	// init settings
	_settingsManager = new SettingsManager(0, this);

	// set inital log lvl if the loglvl wasn't overwritten by arg
	if (!logLvlOverwrite)
		handleSettingsUpdate(settings::LOGGER, getSetting(settings::LOGGER));

	// init EffectFileHandler
	EffectFileHandler *efh = new EffectFileHandler(rootPath, getSetting(settings::EFFECTS), this);
	connect(this, &HyperionDaemon::settingsChanged, efh, &EffectFileHandler::handleSettingsUpdate);

	// connect and apply settings for AuthManager
	connect(this, &HyperionDaemon::settingsChanged, _authManager, &AuthManager::handleSettingsUpdate);
	_authManager->handleSettingsUpdate(settings::NETWORK, _settingsManager->getSetting(settings::NETWORK));

	// connect and apply settings for NetOrigin
	connect(this, &HyperionDaemon::settingsChanged, _netOrigin, &NetOrigin::handleSettingsUpdate);
	_netOrigin->handleSettingsUpdate(settings::NETWORK, _settingsManager->getSetting(settings::NETWORK));

	// spawn all Hyperion instances (non blocking)
	_instanceManager->startAll();

	//Cleaning up Hyperion before quit
	connect(parent, SIGNAL(aboutToQuit()), this, SLOT(freeObjects()));

	// pipe settings changes and component state changes from HyperionIManager to Daemon
	connect(_instanceManager, &HyperionIManager::settingsChanged, this, &HyperionDaemon::settingsChanged);
	connect(_instanceManager, &HyperionIManager::compStateChangeRequest, this, &HyperionDaemon::compStateChangeRequest);

	// listen for setting changes of framegrabber and v4l2
	connect(this, &HyperionDaemon::settingsChanged, this, &HyperionDaemon::handleSettingsUpdate);

	// forward videoModes from HyperionIManager to Daemon evaluation
	connect(_instanceManager, &HyperionIManager::requestVideoMode, this, &HyperionDaemon::setVideoMode);
	// return videoMode changes from Daemon to HyperionIManager
	connect(this, &HyperionDaemon::videoMode, _instanceManager, &HyperionIManager::newVideoMode);

// ---- grabber -----
	Warning(_log, "No platform capture can be instantiated, because all grabbers have been left out from the build");


	// init system capture (framegrabber)
	handleSettingsUpdate(settings::SYSTEMCAPTURE, getSetting(settings::SYSTEMCAPTURE));

	// init v4l2 capture
	handleSettingsUpdate(settings::V4L2, getSetting(settings::V4L2));

	// ---- network services -----
	startNetworkServices();		
}

HyperionDaemon::~HyperionDaemon()
{
	delete _settingsManager;
	delete _pyInit;
}

void HyperionDaemon::setVideoMode(VideoMode mode)
{
	if (_currVideoMode != mode)
	{
		_currVideoMode = mode;
		emit videoMode(mode);
	}
}

QJsonDocument HyperionDaemon::getSetting(settings::type type) const
{
	return _settingsManager->getSetting(type);
}

void HyperionDaemon::freeObjects()
{
	Debug(_log, "Cleaning up Hyperion before quit.");

	// destroy network first as a client might want to access hyperion
	delete _jsonServer;
	_jsonServer = nullptr;

	if (_flatBufferServer)
	{
		auto flatBufferServerThread = _flatBufferServer->thread();
		flatBufferServerThread->quit();
		flatBufferServerThread->wait();
		delete flatBufferServerThread;
		_flatBufferServer = nullptr;
	}

	if (_protoServer)
	{
		auto protoServerThread = _protoServer->thread();
		protoServerThread->quit();
		protoServerThread->wait();
		delete protoServerThread;
		_protoServer = nullptr;
	}

	//ssdp before webserver
	if (_ssdp)
	{
		auto ssdpThread = _ssdp->thread();
		ssdpThread->quit();
		ssdpThread->wait();
		delete ssdpThread;
		_ssdp = nullptr;
	}

	if(_webserver)
	{
		auto webserverThread =_webserver->thread();
		webserverThread->quit();
		webserverThread->wait();
		delete webserverThread;
		_webserver = nullptr;
	}

	if (_sslWebserver)
	{
		auto sslWebserverThread =_sslWebserver->thread();
		sslWebserverThread->quit();
		sslWebserverThread->wait();
		delete sslWebserverThread;
		_sslWebserver = nullptr;
	}

	// stop Hyperions (non blocking)
	_instanceManager->stopAll();

	delete _bonjourBrowserWrapper;
	delete _v4l2Grabber;
	delete _qtcGrabber;

	_v4l2Grabber = nullptr;
	_qtcGrabber = nullptr;
	_bonjourBrowserWrapper = nullptr;
}

void HyperionDaemon::startNetworkServices()
{
	// Create Json server
	_jsonServer = new JsonServer(getSetting(settings::JSONSERVER));
	connect(this, &HyperionDaemon::settingsChanged, _jsonServer, &JsonServer::handleSettingsUpdate);

	// Create FlatBuffer server in thread
	_flatBufferServer = new FlatBufferServer(getSetting(settings::FLATBUFSERVER));
	QThread *fbThread = new QThread(this);
	fbThread->setObjectName("FlatBufferServerThread");
	_flatBufferServer->moveToThread(fbThread);
	connect(fbThread, &QThread::started, _flatBufferServer, &FlatBufferServer::initServer);
	connect(fbThread, &QThread::finished, _flatBufferServer, &FlatBufferServer::deleteLater);
	connect(this, &HyperionDaemon::settingsChanged, _flatBufferServer, &FlatBufferServer::handleSettingsUpdate);
	fbThread->start();

	// Create Proto server in thread
	_protoServer = new ProtoServer(getSetting(settings::PROTOSERVER));
	QThread *pThread = new QThread(this);
	pThread->setObjectName("ProtoServerThread");
	_protoServer->moveToThread(pThread);
	connect(pThread, &QThread::started, _protoServer, &ProtoServer::initServer);
	connect(pThread, &QThread::finished, _protoServer, &ProtoServer::deleteLater);
	connect(this, &HyperionDaemon::settingsChanged, _protoServer, &ProtoServer::handleSettingsUpdate);
	pThread->start();

	// Create Webserver in thread
	_webserver = new WebServer(getSetting(settings::WEBSERVER), false);
	QThread *wsThread = new QThread(this);
	wsThread->setObjectName("WebServerThread");
	_webserver->moveToThread(wsThread);
	connect(wsThread, &QThread::started, _webserver, &WebServer::initServer);
	connect(wsThread, &QThread::finished, _webserver, &WebServer::deleteLater);
	connect(this, &HyperionDaemon::settingsChanged, _webserver, &WebServer::handleSettingsUpdate);
	wsThread->start();

	// Create SSL Webserver in thread
	_sslWebserver = new WebServer(getSetting(settings::WEBSERVER), true);
	QThread *sslWsThread = new QThread(this);
	sslWsThread->setObjectName("SSLWebServerThread");
	_sslWebserver->moveToThread(sslWsThread);
	connect(sslWsThread, &QThread::started, _sslWebserver, &WebServer::initServer);
	connect(sslWsThread, &QThread::finished, _sslWebserver, &WebServer::deleteLater);
	connect(this, &HyperionDaemon::settingsChanged, _sslWebserver, &WebServer::handleSettingsUpdate);
	sslWsThread->start();

	// Create SSDP server in thread
	_ssdp = new SSDPHandler(_webserver, getSetting(settings::FLATBUFSERVER).object()["port"].toInt(), getSetting(settings::JSONSERVER).object()["port"].toInt(), getSetting(settings::GENERAL).object()["name"].toString());
	QThread *ssdpThread = new QThread(this);
	ssdpThread->setObjectName("SSDPThread");
	_ssdp->moveToThread(ssdpThread);
	connect(ssdpThread, &QThread::started, _ssdp, &SSDPHandler::initServer);
	connect(ssdpThread, &QThread::finished, _ssdp, &SSDPHandler::deleteLater);
	connect(_webserver, &WebServer::stateChange, _ssdp, &SSDPHandler::handleWebServerStateChange);
	connect(this, &HyperionDaemon::settingsChanged, _ssdp, &SSDPHandler::handleSettingsUpdate);
	ssdpThread->start();
}

void HyperionDaemon::handleSettingsUpdate(settings::type settingsType, const QJsonDocument &config)
{
	if (settingsType == settings::LOGGER)
	{
		const QJsonObject &logConfig = config.object();

		std::string level = logConfig["level"].toString("warn").toStdString(); // silent warn verbose debug
		if (level == "silent")
			Logger::setLogLevel(Logger::OFF);
		else if (level == "warn")
			Logger::setLogLevel(Logger::LogLevel::WARNING);
		else if (level == "verbose")
			Logger::setLogLevel(Logger::INFO);
		else if (level == "debug")
			Logger::setLogLevel(Logger::DEBUG);
	}

	if (settingsType == settings::SYSTEMCAPTURE)
	{
		const QJsonObject &grabberConfig = config.object();

		_grabber_width = grabberConfig["width"].toInt(96);
		_grabber_height = grabberConfig["height"].toInt(96);
		_grabber_frequency = grabberConfig["frequency_Hz"].toInt(10);

		_grabber_cropLeft = grabberConfig["cropLeft"].toInt(0);
		_grabber_cropRight = grabberConfig["cropRight"].toInt(0);
		_grabber_cropTop = grabberConfig["cropTop"].toInt(0);
		_grabber_cropBottom = grabberConfig["cropBottom"].toInt(0);

		_grabber_ge2d_mode = grabberConfig["ge2d_mode"].toInt(0);
		_grabber_device = grabberConfig["amlogic_grabber"].toString("amvideocap0");

		QString type = grabberConfig["type"].toString("auto");


		// auto eval of type
		if (type == "auto")
		{
			// dispmanx -> on raspi
			if (QFile::exists("/dev/vchiq"))
			{
				type = "dispmanx";
			}
			// amlogic -> /dev/amvideo exists
			else if (QFile::exists("/dev/amvideo"))
			{
				type = "amlogic";

				if (!QFile::exists("/dev/" + _grabber_device))
				{
					Error(_log, "grabber device '%s' for type amlogic not found!", QSTRING_CSTR(_grabber_device));
				}
			}
			else
			{
				// x11 -> if DISPLAY is set
				QByteArray envDisplay = qgetenv("DISPLAY");
				if ( !envDisplay.isEmpty() )
				{
				
				}
				// qt -> if nothing other applies
				else
				{
					type = "qt";
				}
			}
		}

		if (_prevType != type)
		{
			Info(_log, "set screen capture device to '%s'", QSTRING_CSTR(type));

			// stop all capture interfaces

			// create/start capture interface
			{
				Error(_log, "Unknown platform capture type: %s", QSTRING_CSTR(type));
				return;
			}
			_prevType = type;
		}
	}
	else if (settingsType == settings::V4L2)
	{
		const QJsonObject &grabberConfig = config.object();

#ifdef ENABLE_QTC
		if (_qtcGrabber == nullptr)
		{		
			_qtcGrabber = new QTCWrapper(
					grabberConfig["device"].toString("auto"),
					grabberConfig["width"].toInt(0),
					grabberConfig["height"].toInt(0),
					grabberConfig["fps"].toInt(15),
					grabberConfig["input"].toInt(-1),
					parseVideoStandard(grabberConfig["standard"].toString("no-change")),
					parsePixelFormat(grabberConfig["pixelFormat"].toString("no-change")),
					_rootPath);
					
			// HDR stuff		
			if (!grabberConfig["hdrToneMapping"].toBool(false))	
			{
				_qtcGrabber->setHdrToneMappingEnabled(0);
			}
			else
			{
				_qtcGrabber->setHdrToneMappingEnabled(grabberConfig["hdrToneMappingMode"].toInt(1));
			}
			
			Debug(_log, "QTC grabber created");
			// software frame skipping
			_qtcGrabber->setFpsSoftwareDecimation(grabberConfig["fpsSoftwareDecimation"].toInt(1));
			_qtcGrabber->setEncoding(grabberConfig["v4l2Encoding"].toString("NONE"));
			
			_qtcGrabber->setSignalThreshold(
					grabberConfig["redSignalThreshold"].toDouble(0.0) / 100.0,
					grabberConfig["greenSignalThreshold"].toDouble(0.0) / 100.0,
					grabberConfig["blueSignalThreshold"].toDouble(0.0) / 100.0);
			_qtcGrabber->setCropping(
					grabberConfig["cropLeft"].toInt(0),
					grabberConfig["cropRight"].toInt(0),
					grabberConfig["cropTop"].toInt(0),
					grabberConfig["cropBottom"].toInt(0));

			_qtcGrabber->setCecDetectionEnable(grabberConfig["cecDetection"].toBool(true));
			_qtcGrabber->setSignalDetectionEnable(grabberConfig["signalDetection"].toBool(true));
			_qtcGrabber->setSignalDetectionOffset(
					grabberConfig["sDHOffsetMin"].toDouble(0.25),
					grabberConfig["sDVOffsetMin"].toDouble(0.25),
					grabberConfig["sDHOffsetMax"].toDouble(0.75),
					grabberConfig["sDVOffsetMax"].toDouble(0.75));
			Debug(_log, "QTC grabber created");

			// connect to HyperionDaemon signal
			connect(this, &HyperionDaemon::videoMode, _qtcGrabber, &QTCWrapper::setVideoMode);
			connect(this, &HyperionDaemon::settingsChanged, _qtcGrabber, &QTCWrapper::handleSettingsUpdate);
		}
#else
		Error(_log, "The QTC grabber can not be instantiated, because it has been left out from the build");
#endif


#ifdef ENABLE_V4L2
		if (_v4l2Grabber != nullptr)
			return;
		
		_v4l2Grabber = new V4L2Wrapper(
				grabberConfig["device"].toString("auto"),
				grabberConfig["width"].toInt(0),
				grabberConfig["height"].toInt(0),
				grabberConfig["fps"].toInt(15),
				grabberConfig["input"].toInt(-1),
				parseVideoStandard(grabberConfig["standard"].toString("no-change")),
				parsePixelFormat(grabberConfig["pixelFormat"].toString("no-change")),
				_rootPath);
				
		// HDR stuff		
		if (!grabberConfig["hdrToneMapping"].toBool(false))	
		{
			_v4l2Grabber->setHdrToneMappingEnabled(0);
		}
		else
		{
			_v4l2Grabber->setHdrToneMappingEnabled(grabberConfig["hdrToneMappingMode"].toInt(1));
		}
		
		Debug(_log, "V4L2 grabber created");
		// software frame skipping
		_v4l2Grabber->setFpsSoftwareDecimation(grabberConfig["fpsSoftwareDecimation"].toInt(1));
		_v4l2Grabber->setEncoding(grabberConfig["v4l2Encoding"].toString("NONE"));
		
		_v4l2Grabber->setSignalThreshold(
				grabberConfig["redSignalThreshold"].toDouble(0.0) / 100.0,
				grabberConfig["greenSignalThreshold"].toDouble(0.0) / 100.0,
				grabberConfig["blueSignalThreshold"].toDouble(0.0) / 100.0);
		_v4l2Grabber->setCropping(
				grabberConfig["cropLeft"].toInt(0),
				grabberConfig["cropRight"].toInt(0),
				grabberConfig["cropTop"].toInt(0),
				grabberConfig["cropBottom"].toInt(0));

		_v4l2Grabber->setCecDetectionEnable(grabberConfig["cecDetection"].toBool(true));
		_v4l2Grabber->setSignalDetectionEnable(grabberConfig["signalDetection"].toBool(true));
		_v4l2Grabber->setSignalDetectionOffset(
				grabberConfig["sDHOffsetMin"].toDouble(0.25),
				grabberConfig["sDVOffsetMin"].toDouble(0.25),
				grabberConfig["sDHOffsetMax"].toDouble(0.75),
				grabberConfig["sDVOffsetMax"].toDouble(0.75));
		Debug(_log, "V4L2 grabber created");

		// connect to HyperionDaemon signal
		connect(this, &HyperionDaemon::videoMode, _v4l2Grabber, &V4L2Wrapper::setVideoMode);
		connect(this, &HyperionDaemon::settingsChanged, _v4l2Grabber, &V4L2Wrapper::handleSettingsUpdate);
#else
		Error(_log, "The v4l2 grabber can not be instantiated, because it has been left out from the build");
#endif
	}
}
