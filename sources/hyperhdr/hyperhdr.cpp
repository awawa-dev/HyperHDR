#include <cassert>
#include <stdlib.h>

#include <QTimer>
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

#include <HyperhdrConfig.h> // Required to determine the cmake options

#ifdef USE_STATIC_QT_PLUGINS
	#include <QtPlugin>
	Q_IMPORT_PLUGIN(QJpegPlugin)
	Q_IMPORT_PLUGIN(QGifPlugin)
#endif

// bonjour browser
#ifdef ENABLE_BONJOUR
#include <bonjour/bonjourbrowserwrapper.h>
#endif

#include <jsonserver/JsonServer.h>
#include <webserver/WebServer.h>

// Flatbuffer Server
#include <flatbufserver/FlatBufferServer.h>

// ProtoNanoBuffer Server
#include <proto-nano-server/ProtoServer.h>

// ssdp
#include <ssdp/SSDPHandler.h>

// settings
#include <base/SettingsManager.h>

// AuthManager
#include <base/AuthManager.h>

// InstanceManager HyperHDR
#include <base/HyperHdrIManager.h>

// NetOrigin checks
#include <utils/NetOrigin.h>


// EffectFileHandler
#include <effectengine/EffectDBHandler.h>

#ifdef ENABLE_SOUNDCAPWINDOWS
#include <grabber/SoundCapWindows.h>
#endif

#ifdef ENABLE_SOUNDCAPLINUX
#include <grabber/SoundCapLinux.h>
#endif

#ifdef ENABLE_SOUNDCAPMACOS
#include <grabber/SoundCapMacOS.h>
#endif

#include <utils/PerformanceCounters.h>

#include "hyperhdr.h"

HyperHdrDaemon* HyperHdrDaemon::daemon = nullptr;

HyperHdrDaemon::HyperHdrDaemon(const QString& rootPath, QObject* parent, bool logLvlOverwrite, bool readonlyMode, QStringList params)
	: QObject(parent), _log(Logger::getInstance("DAEMON"))
	, _instanceManager(new HyperHdrIManager(rootPath, this, readonlyMode))
	, _authManager(new AuthManager(this, readonlyMode))
#ifdef ENABLE_BONJOUR
	, _bonjourBrowserWrapper(new BonjourBrowserWrapper())
#endif
	, _netOrigin(new NetOrigin(this))
	, _webserver(nullptr)
	, _sslWebserver(nullptr)
	, _jsonServer(nullptr)
	, _v4l2Grabber(nullptr)
	, _mfGrabber(nullptr)
	, _avfGrabber(nullptr)
	, _macGrabber(nullptr)
	, _dxGrabber(nullptr)
	, _x11Grabber(nullptr)
	, _fbGrabber(nullptr)
	, _pipewireGrabber(nullptr)
	, _cecHandler(nullptr)
	, _ssdp(nullptr)
	, _flatBufferServer(nullptr)
	, _mqtt(nullptr)
#if defined(ENABLE_PROTOBUF)
	, _protoServer(nullptr)
#endif
#if defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPLINUX) || defined(ENABLE_SOUNDCAPMACOS)
	, _snd(nullptr)
#endif
	, _suspendHandler(nullptr)
	, _rootPath(rootPath)
	, _params(params)
{
	HyperHdrDaemon::daemon = this;

	// Register metas for thread queued connection
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");
	qRegisterMetaType<hyperhdr::Components>("hyperhdr::Components");
	qRegisterMetaType<settings::type>("settings::type");
	qRegisterMetaType<QMap<quint8, QJsonObject>>("QMap<quint8,QJsonObject>");
	qRegisterMetaType<std::vector<ColorRgb>>("std::vector<ColorRgb>");

	// init settings
	_settingsManager = new SettingsManager(0, this, readonlyMode);

	// set inital log lvl if the loglvl wasn't overwritten by arg
	if (!logLvlOverwrite)
	{
		handleSettingsUpdate(settings::type::LOGGER, getSetting(settings::type::LOGGER));
	}

	// performance counter
	PerformanceCounters::getInstance();

#if defined(ENABLE_SOUNDCAPWINDOWS)
	// init SoundHandler
	_snd = new SoundCapWindows(getSetting(settings::type::SNDEFFECT), this);
	connect(this, &HyperHdrDaemon::settingsChanged, _snd, &SoundCapWindows::handleSettingsUpdate);
#elif defined(ENABLE_SOUNDCAPLINUX)
	// init SoundHandler
	_snd = new SoundCapLinux(getSetting(settings::type::SNDEFFECT), this);
	connect(this, &HyperHdrDaemon::settingsChanged, _snd, &SoundCapLinux::handleSettingsUpdate);
#elif defined(ENABLE_SOUNDCAPMACOS)
	_snd = new SoundCapMacOS(getSetting(settings::type::SNDEFFECT), this);
	connect(this, &HyperHdrDaemon::settingsChanged, _snd, &SoundCapMacOS::handleSettingsUpdate);
#endif		
	// init EffectFileHandler
	EffectDBHandler* efh = new EffectDBHandler(rootPath, getSetting(settings::type::EFFECTS), this);
	connect(this, &HyperHdrDaemon::settingsChanged, efh, &EffectDBHandler::handleSettingsUpdate);

	// connect and apply settings for AuthManager
	connect(this, &HyperHdrDaemon::settingsChanged, _authManager, &AuthManager::handleSettingsUpdate);
	_authManager->handleSettingsUpdate(settings::type::NETWORK, _settingsManager->getSetting(settings::type::NETWORK));

	// connect and apply settings for NetOrigin
	connect(this, &HyperHdrDaemon::settingsChanged, _netOrigin, &NetOrigin::handleSettingsUpdate);
	_netOrigin->handleSettingsUpdate(settings::type::NETWORK, _settingsManager->getSetting(settings::type::NETWORK));

	// spawn all Hyperhdr instances (non blocking)
	handleSettingsUpdate(settings::type::VIDEOGRABBER, getSetting(settings::type::VIDEOGRABBER));
	handleSettingsUpdate(settings::type::SYSTEMGRABBER, getSetting(settings::type::SYSTEMGRABBER));
	_instanceManager->startAll();

	//Cleaning up Hyperhdr before quit
	connect(parent, SIGNAL(aboutToQuit()), this, SLOT(freeObjects()));

	// pipe settings changes and component state changes from HyperHDRIManager to Daemon
	connect(_instanceManager, &HyperHdrIManager::settingsChanged, this, &HyperHdrDaemon::settingsChanged);
	connect(_instanceManager, &HyperHdrIManager::compStateChangeRequest, this, &HyperHdrDaemon::compStateChangeRequest);
	connect(_instanceManager, &HyperHdrIManager::instanceStateChanged, this, &HyperHdrDaemon::instanceStateChanged);
	connect(_instanceManager, &HyperHdrIManager::settingsChanged, this, &HyperHdrDaemon::handleSettingsUpdateGlobal);

	// listen for setting changes of framegrabber and v4l2
	connect(this, &HyperHdrDaemon::settingsChanged, this, &HyperHdrDaemon::handleSettingsUpdate);

	// ---- network services -----
	startNetworkServices();

	_suspendHandler = std::unique_ptr<SuspendHandler>(new SuspendHandler());

#ifdef _WIN32
	if (QAbstractEventDispatcher::instance() != nullptr)
		QAbstractEventDispatcher::instance()->installNativeEventFilter(_suspendHandler.get());
#endif
}

void HyperHdrDaemon::instanceStateChanged(InstanceState state, quint8 instance, const QString& name)
{
	// start web server if needed
	if (state == InstanceState::H_STARTED)
	{
		if (_instanceManager->areInstancesReady())
		{
			if (_webserver != nullptr && !_webserver->thread()->isRunning())
				_webserver->thread()->start();
			if (_sslWebserver != nullptr && !_sslWebserver->thread()->isRunning())
				_sslWebserver->thread()->start();
		}
	}

	// cec
	updateCEC();
}

HyperHdrDaemon::~HyperHdrDaemon()
{
#if defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPLINUX) || defined(ENABLE_SOUNDCAPMACOS)
	delete _snd;
#endif
	delete _settingsManager;
}

QJsonDocument HyperHdrDaemon::getSetting(settings::type type) const
{
	return _settingsManager->getSetting(type);
}

void HyperHdrDaemon::freeObjects()
{
	Debug(_log, "Cleaning up HyperHdr before quit.");

	// unload cec
	unloadCEC();

#if defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPLINUX) || defined(ENABLE_SOUNDCAPMACOS)
	if (_snd)
		_snd->ForcedClose();
#endif

	// destroy network first as a client might want to access hyperhdr
	delete _jsonServer;
	_jsonServer = nullptr;

	if (_flatBufferServer != nullptr)
	{
		auto flatBufferServerThread = _flatBufferServer->thread();
		flatBufferServerThread->quit();
		flatBufferServerThread->wait();
		delete flatBufferServerThread;
		_flatBufferServer = nullptr;
	}

#if defined(ENABLE_PROTOBUF)
	if (_protoServer != nullptr)
	{
		auto protoServerThread = _protoServer->thread();
		protoServerThread->quit();
		protoServerThread->wait();
		delete protoServerThread;
		_protoServer = nullptr;
	}
#endif

	//ssdp before webserver
	if (_ssdp != nullptr)
	{
		auto ssdpThread = _ssdp->thread();
		ssdpThread->quit();
		ssdpThread->wait();
		delete ssdpThread;
		_ssdp = nullptr;
	}

	if (_webserver != nullptr)
	{
		auto webserverThread = _webserver->thread();
		webserverThread->quit();
		webserverThread->wait();
		delete webserverThread;
		_webserver = nullptr;
	}

	if (_sslWebserver != nullptr)
	{
		auto sslWebserverThread = _sslWebserver->thread();
		sslWebserverThread->quit();
		sslWebserverThread->wait();
		delete sslWebserverThread;
		_sslWebserver = nullptr;
	}

	// stop HyperHDRs (non blocking)
	_instanceManager->stopAll();

#ifdef ENABLE_BONJOUR
	delete _bonjourBrowserWrapper;
	_bonjourBrowserWrapper = nullptr;
#endif	

	delete _mqtt;
	delete _v4l2Grabber;
	delete _mfGrabber;
	delete _dxGrabber;
	delete _avfGrabber;
	delete _macGrabber;
	delete _x11Grabber;
	delete _fbGrabber;
	delete _pipewireGrabber;

	_v4l2Grabber = nullptr;
	_mfGrabber = nullptr;
	_dxGrabber = nullptr;
	_avfGrabber = nullptr;
	_macGrabber = nullptr;
	_x11Grabber = nullptr;
	_fbGrabber = nullptr;
	_pipewireGrabber = nullptr;
}

void HyperHdrDaemon::startNetworkServices()
{
	// Create Json server
	_jsonServer = new JsonServer(getSetting(settings::type::JSONSERVER));
	connect(this, &HyperHdrDaemon::settingsChanged, _jsonServer, &JsonServer::handleSettingsUpdate);

	// Create FlatBuffer server in thread
	_flatBufferServer = new FlatBufferServer(getSetting(settings::type::FLATBUFSERVER), _rootPath);
	QThread* fbThread = new QThread(this);
	fbThread->setObjectName("FlatBufferServerThread");
	_flatBufferServer->moveToThread(fbThread);
	connect(fbThread, &QThread::started, _flatBufferServer, &FlatBufferServer::initServer);
	connect(fbThread, &QThread::finished, _flatBufferServer, &FlatBufferServer::deleteLater);
	connect(this, &HyperHdrDaemon::settingsChanged, _flatBufferServer, &FlatBufferServer::handleSettingsUpdate);
	fbThread->start();

#if defined(ENABLE_PROTOBUF)
	// Create Proto server in thread
	_protoServer = new ProtoServer(getSetting(settings::type::PROTOSERVER));
	QThread* pThread = new QThread(this);
	pThread->setObjectName("ProtoServerThread");
	_protoServer->moveToThread(pThread);
	connect(pThread, &QThread::started, _protoServer, &ProtoServer::initServer);
	connect(pThread, &QThread::finished, _protoServer, &ProtoServer::deleteLater);
	connect(this, &HyperHdrDaemon::settingsChanged, _protoServer, &ProtoServer::handleSettingsUpdate);
	pThread->start();
#endif

	// Create Webserver in thread
	_webserver = new WebServer(getSetting(settings::type::WEBSERVER), false);
	QThread* wsThread = new QThread(this);
	wsThread->setObjectName("WebServerThread");
	_webserver->moveToThread(wsThread);
	connect(wsThread, &QThread::started, _webserver, &WebServer::initServer);
	connect(wsThread, &QThread::finished, _webserver, &WebServer::deleteLater);
	connect(this, &HyperHdrDaemon::settingsChanged, _webserver, &WebServer::handleSettingsUpdate);

	// Create SSL Webserver in thread
	_sslWebserver = new WebServer(getSetting(settings::type::WEBSERVER), true);
	QThread* sslWsThread = new QThread(this);
	sslWsThread->setObjectName("SSLWebServerThread");
	_sslWebserver->moveToThread(sslWsThread);
	connect(sslWsThread, &QThread::started, _sslWebserver, &WebServer::initServer);
	connect(sslWsThread, &QThread::finished, _sslWebserver, &WebServer::deleteLater);
	connect(this, &HyperHdrDaemon::settingsChanged, _sslWebserver, &WebServer::handleSettingsUpdate);

	// Create SSDP server in thread
	_ssdp = new SSDPHandler(_webserver,
		getSetting(settings::type::FLATBUFSERVER).object()["port"].toInt(),
		getSetting(settings::type::PROTOSERVER).object()["port"].toInt(),
		getSetting(settings::type::JSONSERVER).object()["port"].toInt(),
		getSetting(settings::type::WEBSERVER).object()["sslPort"].toInt(),
		getSetting(settings::type::GENERAL).object()["name"].toString());
	QThread* ssdpThread = new QThread(this);
	ssdpThread->setObjectName("SSDPThread");
	_ssdp->moveToThread(ssdpThread);
	connect(ssdpThread, &QThread::started, _ssdp, &SSDPHandler::initServer);
	connect(ssdpThread, &QThread::finished, _ssdp, &SSDPHandler::deleteLater);
	connect(_webserver, &WebServer::stateChange, _ssdp, &SSDPHandler::handleWebServerStateChange);
	connect(this, &HyperHdrDaemon::settingsChanged, _ssdp, &SSDPHandler::handleSettingsUpdate);
	ssdpThread->start();

#ifdef ENABLE_MQTT
	_mqtt = new mqtt(this);
	connect(this, &HyperHdrDaemon::settingsChanged, _mqtt, &mqtt::handleSettingsUpdate);
	emit _mqtt->handleSettingsUpdate(settings::type::WEBSERVER, _settingsManager->getSetting(settings::type::WEBSERVER));
	QTimer::singleShot(1500, [this]() {emit _mqtt->handleSettingsUpdate(settings::type::MQTT, _settingsManager->getSetting(settings::type::MQTT)); });
#endif
}

quint16 HyperHdrDaemon::getWebPort()
{
	return (quint16)getSetting(settings::type::WEBSERVER).object()["port"].toInt(8090);
}

void HyperHdrDaemon::handleSettingsUpdateGlobal(settings::type settingsType, const QJsonDocument& config)
{
	if (settingsType == settings::type::SYSTEMCONTROL || settingsType == settings::type::VIDEOCONTROL || settingsType == settings::type::VIDEOGRABBER)
	{
		// cec
		updateCEC();
	}
}

void HyperHdrDaemon::handleSettingsUpdate(settings::type settingsType, const QJsonDocument& config)
{
	if (settingsType == settings::type::LOGGER)
	{
		const QJsonObject& logConfig = config.object();

		std::string level = logConfig["level"].toString("warn").toStdString(); // silent warn verbose debug
		if (level == "silent")
		{
			Logger::setLogLevel(Logger::OFF);
		}
		else if (level == "warn")
		{
			Logger::setLogLevel(Logger::LogLevel::WARNING);
		}
		else if (level == "verbose")
		{
			Logger::setLogLevel(Logger::INFO);
		}
		else if (level == "debug")
		{
			Logger::setLogLevel(Logger::DEBUG);
		}
	}

	if (settingsType == settings::type::SNDEFFECT)
	{
	}

	if (settingsType == settings::type::VIDEOGRABBER)
	{
		const QJsonObject& grabberConfig = config.object();

#if defined(ENABLE_AVF)

		if (_avfGrabber == nullptr)
		{
			_avfGrabber = new AVFWrapper(grabberConfig["device"].toString("auto"), _rootPath);

			_avfGrabber->handleSettingsUpdate(settings::type::VIDEOGRABBER, config);
			connect(this, &HyperHdrDaemon::settingsChanged, _avfGrabber, &AVFWrapper::handleSettingsUpdate);
		}

#elif  !defined(ENABLE_MF) && !defined(ENABLE_V4L2)
		Warning(_log, "The AVF grabber can not be instantiated, because it has been left out from the build");
#endif


#if defined(ENABLE_MF)
		if (_mfGrabber == nullptr)
		{
			_mfGrabber = new MFWrapper(grabberConfig["device"].toString("auto"), _rootPath);

			_mfGrabber->handleSettingsUpdate(settings::type::VIDEOGRABBER, config);
			connect(this, &HyperHdrDaemon::settingsChanged, _mfGrabber, &MFWrapper::handleSettingsUpdate);
		}
#elif !defined(ENABLE_V4L2) && !defined(ENABLE_AVF)
		Warning(_log, "The MF grabber can not be instantiated, because it has been left out from the build");
#endif


#if defined(ENABLE_V4L2)
		if (_v4l2Grabber == nullptr)
		{
			_v4l2Grabber = new V4L2Wrapper(grabberConfig["device"].toString("auto"), _rootPath);

			_v4l2Grabber->handleSettingsUpdate(settings::type::VIDEOGRABBER, config);
			connect(this, &HyperHdrDaemon::settingsChanged, _v4l2Grabber, &V4L2Wrapper::handleSettingsUpdate);
		}
#elif !defined(ENABLE_MF) && !defined(ENABLE_AVF)
		Warning(_log, "!The v4l2 grabber can not be instantiated, because it has been left out from the build");
#endif

		emit settingsChanged(settings::type::VIDEODETECTION, getSetting(settings::type::VIDEODETECTION));
	}

	if (settingsType == settings::type::SYSTEMGRABBER)
	{
		const QJsonObject& grabberConfig = config.object();

#if defined(ENABLE_MAC_SYSTEM)

		if (_macGrabber == nullptr)
		{
			_macGrabber = new macOsWrapper(grabberConfig["device"].toString("auto"), _rootPath);

			_macGrabber->handleSettingsUpdate(settings::type::SYSTEMGRABBER, config);
			connect(this, &HyperHdrDaemon::settingsChanged, _macGrabber, &macOsWrapper::handleSettingsUpdate);
		}

#elif  !defined(ENABLE_DX) && !defined(ENABLE_X11)
		Warning(_log, "The MACOS system grabber can not be instantiated, because it has been left out from the build");
#endif

#if defined(ENABLE_DX)
		if (_dxGrabber == nullptr)
		{
			_dxGrabber = new DxWrapper(grabberConfig["device"].toString("auto"), _rootPath);

			_dxGrabber->handleSettingsUpdate(settings::type::SYSTEMGRABBER, config);
			connect(this, &HyperHdrDaemon::settingsChanged, _dxGrabber, &DxWrapper::handleSettingsUpdate);
		}
#elif !defined(ENABLE_MAC_SYSTEM) && !defined(ENABLE_X11)
		Warning(_log, "The DX Windows system grabber can not be instantiated, because it has been left out from the build");
#endif

#if defined(ENABLE_PIPEWIRE)
		if (_fbGrabber == nullptr && _x11Grabber == nullptr && _pipewireGrabber == nullptr)
		{
			auto backupInst = SystemWrapper::instance;

			bool force = _params.contains("pipewire");

			if (force)
				Info(_log, "User chose forced Pipewire grabber activation");

			_pipewireGrabber = new PipewireWrapper(grabberConfig["device"].toString("auto"), _rootPath);
			if (_pipewireGrabber->isActivated(force))
			{
				_pipewireGrabber->handleSettingsUpdate(settings::type::SYSTEMGRABBER, config);
				connect(this, &HyperHdrDaemon::settingsChanged, _pipewireGrabber, &PipewireWrapper::handleSettingsUpdate);
			}
			else
			{
				SystemWrapper::instance = backupInst;

				_pipewireGrabber->deleteLater();
				_pipewireGrabber = nullptr;

				Warning(_log, "The system doesn't support the Pipewire/Portal grabber");
			}
		}
#elif !defined(ENABLE_DX) && !defined(ENABLE_MAC_SYSTEM)
		Warning(_log, "The pipewire Linux system grabber can not be instantiated, because it has been left out from the build");
#endif

#if defined(ENABLE_X11)
		if (_fbGrabber == nullptr && _x11Grabber == nullptr && _pipewireGrabber == nullptr)
		{
			auto backupInst = SystemWrapper::instance;

			_x11Grabber = new X11Wrapper(grabberConfig["device"].toString("auto"), _rootPath);

			if (_x11Grabber->isActivated(false))
			{
				_x11Grabber->handleSettingsUpdate(settings::type::SYSTEMGRABBER, config);
				connect(this, &HyperHdrDaemon::settingsChanged, _x11Grabber, &X11Wrapper::handleSettingsUpdate);
			}
			else
			{
				SystemWrapper::instance = backupInst;

				_x11Grabber->deleteLater();
				_x11Grabber = nullptr;

				Warning(_log, "The system doesn't support the X11 grabber");
			}
		}
#elif !defined(ENABLE_DX) && !defined(ENABLE_MAC_SYSTEM)
		Warning(_log, "The X11 Linux system grabber can not be instantiated, because it has been left out from the build");
#endif

#if defined(ENABLE_FRAMEBUFFER)
		if (_fbGrabber == nullptr && _x11Grabber == nullptr && _pipewireGrabber == nullptr)
		{
			auto backupInst = SystemWrapper::instance;

			_fbGrabber = new FrameBufWrapper(grabberConfig["device"].toString("auto"), _rootPath);

			if (_fbGrabber->isActivated(false))
			{
				_fbGrabber->handleSettingsUpdate(settings::type::SYSTEMGRABBER, config);
				connect(this, &HyperHdrDaemon::settingsChanged, _fbGrabber, &FrameBufWrapper::handleSettingsUpdate);
			}
			else
			{
				SystemWrapper::instance = backupInst;

				_fbGrabber->deleteLater();
				_fbGrabber = nullptr;

				Warning(_log, "The system doesn't support the FrameBuffer grabber");
			}
		}
#elif !defined(ENABLE_DX) && !defined(ENABLE_MAC_SYSTEM)
		Warning(_log, "The FrameBuffer Linux system grabber can not be instantiated, because it has been left out from the build");
#endif
	}
}

void HyperHdrDaemon::updateCEC()
{
	if (_instanceManager->isCEC())
	{
		Info(_log, "Request CEC");
		loadCEC();
	}
	else
	{
		Info(_log, "Unload CEC");
		unloadCEC();
	}
}

void HyperHdrDaemon::loadCEC()
{
#if defined(ENABLE_CEC)
	if (_cecHandler != nullptr)
		return;

	Info(_log, "Opening libCEC library.");

	_cecHandler = new cecHandler();
	connect(_cecHandler, &cecHandler::stateChange, this, &HyperHdrDaemon::enableCEC);
	connect(_cecHandler, &cecHandler::keyPressed, this, &HyperHdrDaemon::keyPressedCEC);
	if (_cecHandler->start())
		Info(_log, "Success: libCEC library loaded.");
	else
	{
		Error(_log, "Could not open libCEC library");
		unloadCEC();
	}
#endif
}

void HyperHdrDaemon::unloadCEC()
{
#if defined(ENABLE_CEC)	
	if (_cecHandler != nullptr)
	{
		disconnect(_cecHandler, &cecHandler::stateChange, this, &HyperHdrDaemon::enableCEC);
		disconnect(_cecHandler, &cecHandler::keyPressed, this, &HyperHdrDaemon::keyPressedCEC);
		Info(_log, "Stopping CEC");
		_cecHandler->stop();
		Info(_log, "Cleaning up CEC");
		delete _cecHandler;
		_cecHandler = nullptr;
	}
#endif
}

void HyperHdrDaemon::enableCEC(bool enabled, QString info)
{
	auto manager = HyperHdrIManager::getInstance();

	if (manager != nullptr)
	{
		Info(_log, "Received CEC command: %s, %s", (enabled) ? "enable" : "disable", QSTRING_CSTR(info));
		manager->setSignalStateByCEC(enabled);
	}
}

void HyperHdrDaemon::keyPressedCEC(int keyCode)
{
#if defined(ENABLE_V4L2)
	emit GrabberWrapper::getInstance()->cecKeyPressed(keyCode);
#endif
}
