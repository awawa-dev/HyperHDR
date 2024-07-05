#ifndef PCH_ENABLED
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

	#include <cassert>
	#include <stdlib.h>
#endif

#include <QCoreApplication>
#include <utils/Components.h>
#include <image/Image.h>
#include <utils/GlobalSignals.h>
#include <base/HyperHdrInstance.h>
#include <base/GrabberWrapper.h>
#include <base/GrabberHelper.h>
#include <base/NetworkForwarder.h>

#include <jsonserver/JsonServer.h>
#include <webserver/WebServer.h>

// Flatbuffer Server
#include <flatbuffers/server/FlatBuffersServer.h>

// ProtoNanoBuffer Server
#include <proto-nano-server/ProtoServer.h>

// ssdp
#include <ssdp/SSDPHandler.h>

// instance zero default configuration
#include <base/InstanceConfig.h>

// AccessManager
#include <base/AccessManager.h>

// InstanceManager HyperHDR
#include <base/HyperHdrManager.h>

// NetOrigin checks
#include <utils/NetOrigin.h>

#include <performance-counters/PerformanceCounters.h>

#include "HyperHdrDaemon.h"

HyperHdrDaemon::HyperHdrDaemon(const QString& rootPath, QCoreApplication* parent, bool logLvlOverwrite, bool readonlyMode, QStringList params, bool isGuiApp)
	: QObject(parent)
	, _log(Logger::getInstance("DAEMON"))
	, _instanceManager(nullptr)
	, _accessManager(nullptr)
	, _netOrigin(nullptr)
	, _videoGrabber(nullptr)
	, _systemGrabber(nullptr)
	, _soundCapture(nullptr)
	, _performanceCounters(nullptr)
	, _discoveryWrapper(nullptr)
	, _flatProtoBuffersThread(nullptr)
	, _networkThread(nullptr)
	, _mqttThread(nullptr)
	, _suspendHandler(nullptr)
	, _wrapperCEC(nullptr)
	, _rootPath(rootPath)
	, _params(params)
	, _isGuiApp(isGuiApp)
	, _disableOnStart(false)
{

	// Register metas for thread queued connection
	qRegisterMetaType<ColorRgb>("ColorRgb");
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");
	qRegisterMetaType<hyperhdr::Components>("hyperhdr::Components");
	qRegisterMetaType<settings::type>("settings::type");
	qRegisterMetaType<QMap<quint8, QJsonObject>>("QMap<quint8,QJsonObject>");
	qRegisterMetaType<std::vector<ColorRgb>>("std::vector<ColorRgb>");

	// First load default configuration for other objects
	_instanceZeroConfig = std::unique_ptr<InstanceConfig>(new InstanceConfig(true, 0, this));

	// Instance manager
	_instanceManager = std::shared_ptr<HyperHdrManager>(new HyperHdrManager(rootPath),
		[](HyperHdrManager* instanceManager) {
			SMARTPOINTER_MESSAGE("HyperHdrManager");
			delete instanceManager;
		});
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalGetInstanceManager, this, &HyperHdrDaemon::getInstanceManager, Qt::DirectConnection);

	// Access Manager
	_accessManager = std::shared_ptr<AccessManager>(new AccessManager(this),
		[](AccessManager* accessManager) {
			SMARTPOINTER_MESSAGE("AccessManager");
			delete accessManager;
		});
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalGetAccessManager, this, &HyperHdrDaemon::getAccessManager, Qt::DirectConnection);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _accessManager.get(), &AccessManager::handleSettingsUpdate);
	_accessManager->handleSettingsUpdate(settings::type::NETWORK, getSetting(settings::type::NETWORK));

	// Performance Counters
	_performanceCounters = std::shared_ptr<PerformanceCounters>(new PerformanceCounters(),
		[](PerformanceCounters* performanceCounters) {
			SMARTPOINTER_MESSAGE("PerformanceCounters");
			delete performanceCounters;
		});
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalGetPerformanceCounters, this, &HyperHdrDaemon::getPerformanceCounters, Qt::DirectConnection);

	// Discovery Wrapper
	#ifdef ENABLE_BONJOUR
		_discoveryWrapper = std::shared_ptr<DiscoveryWrapper>(new DiscoveryWrapper(this),
		[](DiscoveryWrapper* bonjourBrowserWrapper) {
			SMARTPOINTER_MESSAGE("DiscoveryWrapper");
			delete bonjourBrowserWrapper;
		});	
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalGetDiscoveryWrapper, this, &HyperHdrDaemon::getDiscoveryWrapper, Qt::DirectConnection);
	#endif

	_netOrigin = std::shared_ptr<NetOrigin>(new NetOrigin(),
		[](NetOrigin* netOrigin) {
			SMARTPOINTER_MESSAGE("NetOrigin");
			delete netOrigin;
		});
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _netOrigin.get(), &NetOrigin::settingsChangedHandler);
	_netOrigin->settingsChangedHandler(settings::type::NETWORK, getSetting(settings::type::NETWORK));

	// set inital log lvl if the loglvl wasn't overwritten by arg
	if (!logLvlOverwrite)
	{
		settingsChangedHandler(settings::type::LOGGER, getSetting(settings::type::LOGGER));
	}

	#if defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPLINUX) || defined(ENABLE_SOUNDCAPMACOS)
		_soundCapture = std::shared_ptr<SoundCapture>(
			new SoundGrabber(getSetting(settings::type::SNDEFFECT), this),
			[](SoundGrabber* soundGrabber) {
				SMARTPOINTER_MESSAGE("SoundGrabber");
				delete soundGrabber;
			});
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalGetSoundCapture, this, &HyperHdrDaemon::getSoundCapture, Qt::DirectConnection);
		connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _soundCapture.get(), &SoundGrabber::settingsChangedHandler);
	#endif


	// CEC wrapper
	#ifdef ENABLE_CEC
		_wrapperCEC = std::unique_ptr<WrapperCEC>(new WrapperCEC());
		connect(_wrapperCEC.get(), &WrapperCEC::SignalStateChange, _instanceManager.get(), &HyperHdrManager::setSignalStateByCEC);
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalRequestComponent, _wrapperCEC.get(), &WrapperCEC::sourceRequestHandler);
	#endif

	// spawn all Hyperhdr instances (non blocking)
	settingsChangedHandler(settings::type::VIDEOGRABBER, getSetting(settings::type::VIDEOGRABBER));
	settingsChangedHandler(settings::type::SYSTEMGRABBER, getSetting(settings::type::SYSTEMGRABBER));
	QJsonObject genConfig = getSetting(settings::type::GENERAL).object();
	_disableOnStart = genConfig["disableLedsStartup"].toBool(false);
	_instanceManager->startAll(_disableOnStart);

	//Cleaning up Hyperhdr before quit
	connect(parent, &QCoreApplication::aboutToQuit, this, &HyperHdrDaemon::freeObjects);

	// pipe settings changes and component state changes from HyperHDRIManager to Daemon
	connect(_instanceManager.get(), &HyperHdrManager::SignalInstanceStateChanged, this, &HyperHdrDaemon::instanceStateChangedHandler);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, this, &HyperHdrDaemon::settingsChangedHandler);

	// ---- network services -----
	startNetworkServices();
}

HyperHdrDaemon::~HyperHdrDaemon() = default;

void HyperHdrDaemon::instanceStateChangedHandler(InstanceState state, quint8 instance, const QString& name)
{
	// start web server if needed
	if (state == InstanceState::START)
	{
		if (_instanceManager->areInstancesReady())
		{
			// power management
			#if defined(HAVE_POWER_MANAGEMENT)
				if (_suspendHandler == nullptr)
				{
					QJsonObject genConfig = getSetting(settings::type::GENERAL).object();
					bool lockedEnable = genConfig["disableOnLocked"].toBool(false);

					_suspendHandler = std::unique_ptr<SuspendHandler>(new SuspendHandler(lockedEnable));
					connect(_suspendHandler.get(), &SuspendHandler::SignalHibernate, _instanceManager.get(), &HyperHdrManager::hibernate);

					#ifdef _WIN32
						if (QAbstractEventDispatcher::instance() != nullptr)
							QAbstractEventDispatcher::instance()->installNativeEventFilter(_suspendHandler.get());
					#endif
				}
			#endif

			if (_systemGrabber != nullptr)
			{
				_systemGrabber->linker.acquire(1);
			}

			if (_videoGrabber != nullptr)
			{
				_videoGrabber->linker.acquire(1);
			}

			if (_flatProtoBuffersThread != nullptr && !_flatProtoBuffersThread->isRunning())
			{
				_flatProtoBuffersThread->start();
			}

			if (_mqttThread != nullptr && !_mqttThread->isRunning())
			{
				_mqttThread->start();
			}

			if (_networkThread != nullptr && !_networkThread->isRunning())
			{
				_networkThread->start();
			}

			if (_disableOnStart)
			{
				Warning(_log, "The user has disabled LEDs auto-start in the configuration (interface: 'General' tab)");
				_instanceManager->toggleStateAllInstances(false);
			}
		}
	}
}

void HyperHdrDaemon::freeObjects()
{
	Info(_log, "Cleaning up HyperHdr before quit [preparing]");

	disconnect(GlobalSignals::getInstance(), nullptr, nullptr, nullptr);
	disconnect(_instanceManager.get(), nullptr, nullptr, nullptr);
	HyperHdrInstance::signalTerminateTriggered();

	Info(_log, "Releasing SmartPointer: CEC [ 1/15]");
	_wrapperCEC = nullptr;

	Info(_log, "Releasing SmartPointer: SuspendHandler [ 2/15]");
	#if defined(_WIN32) && defined(HAVE_POWER_MANAGEMENT)
		if (QAbstractEventDispatcher::instance() != nullptr && _suspendHandler != nullptr)
			QAbstractEventDispatcher::instance()->removeNativeEventFilter(_suspendHandler.get());
	#endif
	_suspendHandler = nullptr;

	Info(_log, "Releasing SmartPointer: FlatBuffer/ProtoServer/Forwarder [ 3/15]");
	_flatProtoBuffersThread = nullptr;

	Info(_log, "Releasing SmartPointer: MQTT [ 4/15]");
	_mqttThread = nullptr;

	Info(_log, "Releasing SmartPointer: WebServers/SSDP/JSonServer [ 5/15]");
	_networkThread = nullptr;

	Info(_log, "Stopping: InstanceManager [ 6/15]");
	_instanceManager->stopAllonExit();

	Info(_log, "Releasing SmartPointer: SoundGrabber [ 7/15]");
	_soundCapture = nullptr;

	Info(_log, "Releasing SmartPointer: BonjourBrowserWrapper [ 8/15]");
	_discoveryWrapper = nullptr;

	Info(_log, "Releasing SmartPointer: VideoGrabber [ 9/15]");
	_videoGrabber = nullptr;

	Info(_log, "Releasing SmartPointer: SoftwareGrabber [10/15]");
	_systemGrabber = nullptr;

	Info(_log, "Releasing SmartPointer: InstanceZeroConfiguration [11/15]");
	_instanceZeroConfig = nullptr;

	Info(_log, "Releasing SmartPointer: InstanceManager [12/15]");
	_instanceManager = nullptr;

	Info(_log, "Releasing SmartPointer: AccessManager [13/15]");
	_accessManager = nullptr;

	Info(_log, "Releasing SmartPointer: PerformanceCounters [14/15]");
	_performanceCounters = nullptr;

	Info(_log, "Releasing SmartPointer: NetOrigin [15/15]");
	_netOrigin = nullptr;

	Info(_log, "Resources are freed [finished]");
}

void HyperHdrDaemon::startNetworkServices()
{
	//////////////////////////////////////////
	//		PROTO/FLATBUFFERS THREAD		//
	//////////////////////////////////////////

	QThread* _flatProtoThread = new QThread();
	std::vector<QObject*> flatProtoThreadClients;

	_flatProtoThread->setObjectName("FlatProtoThread");

	FlatBuffersServer* _flatBufferServer = new FlatBuffersServer(_netOrigin, getSetting(settings::type::FLATBUFSERVER), _rootPath);
	_flatBufferServer->moveToThread(_flatProtoThread);
	flatProtoThreadClients.push_back(_flatBufferServer);
	if (_videoGrabber == nullptr)
	{
		Warning(_log, "The USB grabber was disabled during build. FlatbufferServer now controlls the HDR state.");
		connect(_flatBufferServer, &FlatBuffersServer::SignalSetNewComponentStateToAllInstances, _instanceManager.get(), &HyperHdrManager::SignalSetNewComponentStateToAllInstances);
	}
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalRequestComponent, _flatBufferServer, &FlatBuffersServer::signalRequestSourceHandler);
	connect(_flatProtoThread, &QThread::started, _flatBufferServer, &FlatBuffersServer::initServer);
	connect(_flatProtoThread, &QThread::finished, _flatBufferServer, &FlatBuffersServer::deleteLater);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _flatBufferServer, &FlatBuffersServer::handleSettingsUpdate);

	NetworkForwarder* _networkForwarder = new NetworkForwarder();
	_networkForwarder->moveToThread(_flatProtoThread);
	connect(_flatProtoThread, &QThread::started, _networkForwarder, &NetworkForwarder::startedHandler);
	connect(_flatProtoThread, &QThread::finished, _networkForwarder, &NetworkForwarder::deleteLater);
	flatProtoThreadClients.push_back(_networkForwarder);

#if defined(ENABLE_PROTOBUF)
	ProtoServer* _protoServer = new ProtoServer(_netOrigin, getSetting(settings::type::PROTOSERVER));
	_protoServer->moveToThread(_flatProtoThread);
	flatProtoThreadClients.push_back(_protoServer);
	connect(_flatProtoThread, &QThread::started, _protoServer, &ProtoServer::initServer);
	connect(_flatProtoThread, &QThread::finished, _protoServer, &ProtoServer::deleteLater);
	connect(_protoServer, &ProtoServer::SignalImportFromProto, _flatBufferServer, &FlatBuffersServer::SignalImportFromProto);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _protoServer, &ProtoServer::handleSettingsUpdate);
#endif

	_flatProtoBuffersThread = std::unique_ptr<QThread, std::function<void(QThread*)>>(
		_flatProtoThread, [flatProtoThreadClients](QThread* protoThread) {
			THREAD_MULTI_REMOVER(QString("FlatProtoBufferThread and children"), protoThread, flatProtoThreadClients);
		});

	/////////////////////////////////////////
	//			NETWORK THREAD			   //
	/////////////////////////////////////////
	QThread* _netThread = new QThread();
	std::vector<QObject*> networkThreadClients;

	_netThread->setObjectName("NetworkThread");

	JsonServer* _jsonServer = new JsonServer(_netOrigin, getSetting(settings::type::JSONSERVER));
	_jsonServer->moveToThread(_netThread);
	networkThreadClients.push_back(_jsonServer);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _jsonServer, &JsonServer::handleSettingsUpdate);

	WebServer* _webserver = new WebServer(_netOrigin, getSetting(settings::type::WEBSERVER), false);
	_webserver->moveToThread(_netThread);
	networkThreadClients.push_back(_webserver);
	connect(_netThread, &QThread::started, _webserver, &WebServer::initServer);
	connect(_netThread, &QThread::finished, _webserver, &WebServer::deleteLater);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _webserver, &WebServer::handleSettingsUpdate);

	WebServer* _sslWebserver = new WebServer(_netOrigin, getSetting(settings::type::WEBSERVER), true);
	_sslWebserver->moveToThread(_netThread);
	networkThreadClients.push_back(_sslWebserver);
	connect(_netThread, &QThread::started, _sslWebserver, &WebServer::initServer);
	connect(_netThread, &QThread::finished, _sslWebserver, &WebServer::deleteLater);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _sslWebserver, &WebServer::handleSettingsUpdate);

	SSDPHandler* _ssdp = new SSDPHandler(
		_accessManager->getID(),
		getSetting(settings::type::FLATBUFSERVER).object()["port"].toInt(),
		getSetting(settings::type::PROTOSERVER).object()["port"].toInt(),
		getSetting(settings::type::JSONSERVER).object()["port"].toInt(),
		getSetting(settings::type::WEBSERVER).object()["sslPort"].toInt(),
		getSetting(settings::type::WEBSERVER).object()["port"].toInt(),
		getSetting(settings::type::GENERAL).object()["name"].toString());
	_ssdp->moveToThread(_netThread);
	networkThreadClients.push_back(_ssdp);
	connect(_ssdp, &SSDPHandler::newSsdpXmlDesc, _webserver, &WebServer::setSsdpXmlDesc);
	connect(_netThread, &QThread::started, _ssdp, &SSDPHandler::initServer);
	connect(_netThread, &QThread::finished, _ssdp, &SSDPHandler::deleteLater);
	connect(_webserver, &WebServer::stateChange, _ssdp, &SSDPHandler::handleWebServerStateChange);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _ssdp, &SSDPHandler::handleSettingsUpdate);

	_networkThread = std::unique_ptr<QThread, std::function<void(QThread*)>>(_netThread,
		[networkThreadClients](QThread* networkThread) {
			THREAD_MULTI_REMOVER(QString("NetworkThread and children"), networkThread, networkThreadClients);
		});

#ifdef ENABLE_MQTT
	QThread* mqttThread = new QThread();
	mqtt* _mqtt = new mqtt(getSetting(settings::type::MQTT));
	_mqtt->moveToThread(mqttThread);
	connect(mqttThread, &QThread::started, _mqtt, &mqtt::begin);
	connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, _mqtt, &mqtt::handleSettingsUpdate);
	connect(mqttThread, &QThread::finished, _mqtt, &mqtt::deleteLater);

	_mqttThread = std::unique_ptr<QThread, std::function<void(QThread*)>>(mqttThread,
		[_mqtt](QThread* mqttThread) {
			THREAD_REMOVER(QString("MQTT"), mqttThread, _mqtt);
		});
#endif
}


QJsonDocument HyperHdrDaemon::getSetting(settings::type type) const
{
	if (_instanceZeroConfig == nullptr)
	{
		Error(_log, "Default configuration is not initialized");
		return QJsonDocument();
	}
	else
		return _instanceZeroConfig->getSetting(type);
}

quint16 HyperHdrDaemon::getWebPort()
{
	return (quint16)getSetting(settings::type::WEBSERVER).object()["port"].toInt(8090);
}

template<typename T>
void HyperHdrDaemon::createVideoGrabberHelper(QJsonDocument config, QString deviceName, QString rootPath)
{
	auto videoDetection = getSetting(settings::type::VIDEODETECTION);

	_videoGrabber = std::shared_ptr<GrabberHelper>(
			new GrabberHelper(),
			[](GrabberHelper* vgrabber) {
				THREAD_REMOVER(QString("VideoGrabber"), vgrabber->thread(), vgrabber);
			}
	);

	QThread* _videoGrabberThread = new QThread();

	_videoGrabber->moveToThread(_videoGrabberThread);
	connect(_videoGrabberThread, &QThread::started, _videoGrabber.get(), &GrabberHelper::SignalCreateGrabber);
	connect(_videoGrabberThread, &QThread::finished, _videoGrabber.get(), &GrabberHelper::deleteLater);
	connect(_videoGrabber.get(), &GrabberHelper::SignalCreateGrabber, _videoGrabber.get(), [this, videoDetection, config, deviceName, rootPath]() {
		_videoGrabber->linker.release(1);
		auto videoGrabberInstance = new T(deviceName, rootPath);
		_videoGrabber->setGrabber(videoGrabberInstance);
		connect(videoGrabberInstance, &GrabberWrapper::SignalSetNewComponentStateToAllInstances, _instanceManager.get(), &HyperHdrManager::SignalSetNewComponentStateToAllInstances);
		connect(videoGrabberInstance, &GrabberWrapper::SignalSaveCalibration, _instanceManager.get(), &HyperHdrManager::saveCalibration);
		connect(_videoGrabber->thread(), &QThread::finished, videoGrabberInstance, &GrabberWrapper::deleteLater);
		connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, videoGrabberInstance, &GrabberWrapper::handleSettingsUpdate);
		connect(_instanceManager.get(), &HyperHdrManager::SignalInstancePauseChanged, videoGrabberInstance, &T::SignalInstancePauseChanged);
		#ifdef ENABLE_CEC
			connect(_wrapperCEC.get(), &WrapperCEC::SignalKeyPressed, videoGrabberInstance, &GrabberWrapper::SignalCecKeyPressed);
		#endif
		videoGrabberInstance->GrabberWrapper::handleSettingsUpdate(settings::type::VIDEOGRABBER, config);
		videoGrabberInstance->GrabberWrapper::handleSettingsUpdate(settings::type::VIDEODETECTION, videoDetection);
		_videoGrabber->linker.release(1);
	});

	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalGetVideoGrabber, this, &HyperHdrDaemon::getVideoGrabber, Qt::DirectConnection);
	_videoGrabberThread->start();
	_videoGrabber->linker.acquire(1);
}

void HyperHdrDaemon::createSoftwareGrabberHelper(QJsonDocument config, QString deviceName, QString rootPath)
{
	_systemGrabber = std::shared_ptr<GrabberHelper>(
			new GrabberHelper(),
			[](GrabberHelper* sgrabber) {
				THREAD_REMOVER(QString("SystemGrabber"), sgrabber->thread(), sgrabber);
			}
	);

	QThread* _systemGrabberThread = new QThread();

	_systemGrabber->moveToThread(_systemGrabberThread);
	connect(_systemGrabberThread, &QThread::started, _systemGrabber.get(), &GrabberHelper::SignalCreateGrabber);
	connect(_systemGrabberThread, &QThread::finished, _systemGrabber.get(), &GrabberHelper::deleteLater);
	connect(_systemGrabber.get(), &GrabberHelper::SignalCreateGrabber, _systemGrabber.get(), [this, config, deviceName, rootPath]() {
		bool force = false;

		_systemGrabber->linker.release(1);
		SystemWrapper* softwareGrabberInstance = nullptr;

#if defined(ENABLE_DX)
		if (softwareGrabberInstance == nullptr)
		{
			auto candidate = new DxWrapper(deviceName, _rootPath);
			if (!candidate->isActivated(force))
			{
				Warning(_log, "The system doesn't support the DirectX11 system grabber");
				candidate->deleteLater();
			}
			else
				softwareGrabberInstance = candidate;
		}
#endif
#if defined(ENABLE_PIPEWIRE)
		bool forcePipewire = _params.contains("pipewire");
		if (softwareGrabberInstance == nullptr && (_isGuiApp || forcePipewire))
		{
			auto candidate = new PipewireWrapper(deviceName, _rootPath);
			if (!candidate->isActivated(force))
			{
				Warning(_log, "The system doesn't support the Pipewire/Portal grabber");
				candidate->deleteLater();
			}
			else
				softwareGrabberInstance = candidate;
		}
#endif
#if defined(ENABLE_X11)
		if (softwareGrabberInstance == nullptr)
		{
			auto candidate = new X11Wrapper(deviceName, _rootPath);
			if (!candidate->isActivated(force))
			{
				Warning(_log, "The system doesn't support the X11 system grabber");
				candidate->deleteLater();
			}
			else
				softwareGrabberInstance = candidate;
		}
#endif
#if defined(ENABLE_FRAMEBUFFER)
		if (softwareGrabberInstance == nullptr)
		{
			auto candidate = new FrameBufWrapper(deviceName, _rootPath);
			if (!candidate->isActivated(force))
			{
				Warning(_log, "The system doesn't support the framebuffer system grabber");
				candidate->deleteLater();
			}
			else
				softwareGrabberInstance = candidate;
		}
#endif
#if defined(ENABLE_MAC_SYSTEM)
		if (softwareGrabberInstance == nullptr)
		{
			auto candidate = new macOsWrapper(deviceName, _rootPath);
			if (!candidate->isActivated(force))
			{
				Warning(_log, "The system doesn't support the macOS system grabber");
				candidate->deleteLater();
			}
			else
				softwareGrabberInstance = candidate;
		}
#endif

		if (softwareGrabberInstance != nullptr)
		{
			_systemGrabber->setGrabber(softwareGrabberInstance);

			connect(_systemGrabber->thread(), &QThread::finished, softwareGrabberInstance, &SystemWrapper::deleteLater);
			connect(_instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, softwareGrabberInstance, &SystemWrapper::handleSettingsUpdate);
			softwareGrabberInstance->SystemWrapper::handleSettingsUpdate(settings::type::SYSTEMGRABBER, config);
		}
		_systemGrabber->linker.release(1);
	});

	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalGetSystemGrabber, this, &HyperHdrDaemon::getSystemGrabber, Qt::DirectConnection);
	_systemGrabberThread->start();
	_systemGrabber->linker.acquire(1);
}

void HyperHdrDaemon::getInstanceManager(std::shared_ptr<HyperHdrManager>& retVal)
{
	retVal = _instanceManager;
};

void HyperHdrDaemon::getAccessManager(std::shared_ptr<AccessManager>& retVal)
{
	retVal = _accessManager;
};

void HyperHdrDaemon::getSoundCapture(std::shared_ptr<SoundCapture>& retVal)
{
	retVal = _soundCapture;
}

void HyperHdrDaemon::getSystemGrabber(std::shared_ptr<GrabberHelper>& systemGrabber)
{
	systemGrabber = _systemGrabber;
}

void HyperHdrDaemon::getVideoGrabber(std::shared_ptr<GrabberHelper>& videoGrabber)
{
	videoGrabber = _videoGrabber;
}

void HyperHdrDaemon::getPerformanceCounters(std::shared_ptr<PerformanceCounters>& performanceCounters)
{
	performanceCounters = _performanceCounters;
}

void HyperHdrDaemon::getDiscoveryWrapper(std::shared_ptr<DiscoveryWrapper>& discoveryWrapper)
{
	discoveryWrapper = _discoveryWrapper;
}

void HyperHdrDaemon::settingsChangedHandler(settings::type settingsType, const QJsonDocument& config)
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

	if (settingsType == settings::type::VIDEOGRABBER && _videoGrabber == nullptr)
	{
		const QJsonObject& grabberConfig = config.object();
		const QString deviceName = grabberConfig["device"].toString("auto");

#if defined(ENABLE_AVF)
		if (_videoGrabber == nullptr)
		{
			createVideoGrabberHelper<AVFWrapper>(config, deviceName, _rootPath);
		}
#elif  !defined(ENABLE_MF) && !defined(ENABLE_V4L2)
		Warning(_log, "The AVF grabber can not be instantiated, because it has been left out from the build");
#endif


#if defined(ENABLE_MF)
		if (_videoGrabber == nullptr)
		{
			createVideoGrabberHelper<MFWrapper>(config, deviceName, _rootPath);
		}
#elif !defined(ENABLE_V4L2) && !defined(ENABLE_AVF)
		Warning(_log, "The MF grabber can not be instantiated, because it has been left out from the build");
#endif


#if defined(ENABLE_V4L2)		
		if (_videoGrabber == nullptr)
		{
			createVideoGrabberHelper<V4L2Wrapper>(config, deviceName, _rootPath);
		}
#elif !defined(ENABLE_MF) && !defined(ENABLE_AVF)
		Warning(_log, "!The v4l2 grabber can not be instantiated, because it has been left out from the build");
#endif
		
	}

	if (settingsType == settings::type::SYSTEMGRABBER && _systemGrabber == nullptr)
	{
		#if defined(ENABLE_DX) || defined(ENABLE_PIPEWIRE) || defined(ENABLE_X11) || defined(ENABLE_FRAMEBUFFER) || defined(ENABLE_MAC_SYSTEM)
			const QJsonObject& grabberConfig = config.object();
			const QString deviceName = grabberConfig["device"].toString("auto");
			createSoftwareGrabberHelper(config, deviceName, _rootPath);
		#endif
	}
}
