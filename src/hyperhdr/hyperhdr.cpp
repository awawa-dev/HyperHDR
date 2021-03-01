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

#include <HyperhdrConfig.h> // Required to determine the cmake options

// bonjour browser
#ifdef ENABLE_AVAHI
#include <bonjour/bonjourbrowserwrapper.h>
#endif
#include <jsonserver/JsonServer.h>
#include <webserver/WebServer.h>
#include "hyperhdr.h"

// Flatbuffer Server
#include <flatbufserver/FlatBufferServer.h>

// Protobuffer Server
#include <protoserver/ProtoServer.h>

// ssdp
#include <ssdp/SSDPHandler.h>

// settings
#include <hyperhdrbase/SettingsManager.h>

// AuthManager
#include <hyperhdrbase/AuthManager.h>

// InstanceManager Hyperion
#include <hyperhdrbase/HyperHdrIManager.h>

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

HyperHdrDaemon *HyperHdrDaemon::daemon = nullptr;

HyperHdrDaemon::HyperHdrDaemon(const QString& rootPath, QObject* parent, bool logLvlOverwrite, bool readonlyMode)
	: QObject(parent), _log(Logger::getInstance("DAEMON"))
	  , _instanceManager(new HyperHdrIManager(rootPath, this, readonlyMode))
	  , _authManager(new AuthManager(this, readonlyMode))
#ifdef ENABLE_AVAHI
	  , _bonjourBrowserWrapper(new BonjourBrowserWrapper())
#endif
	  , _netOrigin(new NetOrigin(this))
	  , _webserver(nullptr)
	  , _sslWebserver(nullptr)
	  , _jsonServer(nullptr)
	  , _v4l2Grabber(nullptr)
	  , _mfGrabber(nullptr)
	  , _ssdp(nullptr)
	#if defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPLINUX)
	  , _snd(nullptr)        
	#endif
	  , _currVideoModeHdr(-1)
	  , _rootPath(rootPath)
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

#if defined(ENABLE_SOUNDCAPWINDOWS)
	// init SoundHandler
	_snd = new SoundCapWindows(getSetting(settings::type::SNDEFFECT), this);
        connect(this, &HyperHdrDaemon::settingsChanged, _snd, &SoundCapWindows::handleSettingsUpdate);
#elif defined(ENABLE_SOUNDCAPLINUX)
        // init SoundHandler
        _snd = new SoundCapLinux(getSetting(settings::type::SNDEFFECT), this);
        connect(this, &HyperHdrDaemon::settingsChanged, _snd, &SoundCapLinux::handleSettingsUpdate);
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
	_instanceManager->startAll();

	//Cleaning up Hyperhdr before quit
	connect(parent, SIGNAL(aboutToQuit()), this, SLOT(freeObjects()));

	// pipe settings changes and component state changes from HyperionIManager to Daemon
	connect(_instanceManager, &HyperHdrIManager::settingsChanged, this, &HyperHdrDaemon::settingsChanged);
	connect(_instanceManager, &HyperHdrIManager::compStateChangeRequest, this, &HyperHdrDaemon::compStateChangeRequest);

	// listen for setting changes of framegrabber and v4l2
	connect(this, &HyperHdrDaemon::settingsChanged, this, &HyperHdrDaemon::handleSettingsUpdate);
	// forward videoModes from HyperionIManager to Daemon evaluation
	connect(_instanceManager, &HyperHdrIManager::requestVideoModeHdr, this, &HyperHdrDaemon::setVideoModeHdr);
	// return videoMode changes from Daemon to HyperionIManager
	connect(this, &HyperHdrDaemon::videoModeHdr, _instanceManager, &HyperHdrIManager::newVideoModeHdr);

	// init v4l2 capture
	handleSettingsUpdate(settings::type::V4L2, getSetting(settings::type::V4L2));

	// ---- network services -----
	startNetworkServices();
}

HyperHdrDaemon::~HyperHdrDaemon()
{
#if defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPLINUX)
	delete _snd;	
#endif
	delete _settingsManager;
}

void HyperHdrDaemon::setVideoModeHdr(int hdr)
{
	if (_currVideoModeHdr != hdr)
	{
		_currVideoModeHdr = hdr;
		emit videoModeHdr(hdr);
	}
}

QJsonDocument HyperHdrDaemon::getSetting(settings::type type) const
{
	return _settingsManager->getSetting(type);
}

void HyperHdrDaemon::freeObjects()
{
	Debug(_log, "Cleaning up HyperHdr before quit.");

	#if defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPLINUX)
		if (_snd)
			_snd->ForcedClose();
	#endif

	// destroy network first as a client might want to access hyperion
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

	if (_protoServer != nullptr)
	{
		auto protoServerThread = _protoServer->thread();
		protoServerThread->quit();
		protoServerThread->wait();
		delete protoServerThread;
		_protoServer = nullptr;
	}

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

	// stop Hyperions (non blocking)
	_instanceManager->stopAll();

#ifdef ENABLE_AVAHI
	delete _bonjourBrowserWrapper;
	_bonjourBrowserWrapper = nullptr;
#endif

	delete _v4l2Grabber;
	delete _mfGrabber;

	_v4l2Grabber = nullptr;
	_mfGrabber = nullptr;
}

void HyperHdrDaemon::startNetworkServices()
{
	// Create Json server
	_jsonServer = new JsonServer(getSetting(settings::type::JSONSERVER));
	connect(this, &HyperHdrDaemon::settingsChanged, _jsonServer, &JsonServer::handleSettingsUpdate);

	// Create FlatBuffer server in thread
	_flatBufferServer = new FlatBufferServer(getSetting(settings::type::FLATBUFSERVER));
	QThread* fbThread = new QThread(this);
	fbThread->setObjectName("FlatBufferServerThread");
	_flatBufferServer->moveToThread(fbThread);
	connect(fbThread, &QThread::started, _flatBufferServer, &FlatBufferServer::initServer);
	connect(fbThread, &QThread::finished, _flatBufferServer, &FlatBufferServer::deleteLater);
	connect(this, &HyperHdrDaemon::settingsChanged, _flatBufferServer, &FlatBufferServer::handleSettingsUpdate);
	fbThread->start();

	// Create Proto server in thread
	_protoServer = new ProtoServer(getSetting(settings::type::PROTOSERVER));
	QThread* pThread = new QThread(this);
	pThread->setObjectName("ProtoServerThread");
	_protoServer->moveToThread(pThread);
	connect(pThread, &QThread::started, _protoServer, &ProtoServer::initServer);
	connect(pThread, &QThread::finished, _protoServer, &ProtoServer::deleteLater);
	connect(this, &HyperHdrDaemon::settingsChanged, _protoServer, &ProtoServer::handleSettingsUpdate);
	pThread->start();

	// Create Webserver in thread
	_webserver = new WebServer(getSetting(settings::type::WEBSERVER), false);
	QThread* wsThread = new QThread(this);
	wsThread->setObjectName("WebServerThread");
	_webserver->moveToThread(wsThread);
	connect(wsThread, &QThread::started, _webserver, &WebServer::initServer);
	connect(wsThread, &QThread::finished, _webserver, &WebServer::deleteLater);
	connect(this, &HyperHdrDaemon::settingsChanged, _webserver, &WebServer::handleSettingsUpdate);
	wsThread->start();

	// Create SSL Webserver in thread
	_sslWebserver = new WebServer(getSetting(settings::type::WEBSERVER), true);
	QThread* sslWsThread = new QThread(this);
	sslWsThread->setObjectName("SSLWebServerThread");
	_sslWebserver->moveToThread(sslWsThread);
	connect(sslWsThread, &QThread::started, _sslWebserver, &WebServer::initServer);
	connect(sslWsThread, &QThread::finished, _sslWebserver, &WebServer::deleteLater);
	connect(this, &HyperHdrDaemon::settingsChanged, _sslWebserver, &WebServer::handleSettingsUpdate);
	sslWsThread->start();

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

	if (settingsType == settings::type::V4L2)
	{
		const QJsonObject &grabberConfig = config.object();

#if defined(ENABLE_WMF)
		if (_mfGrabber == nullptr)
		{		
			_mfGrabber = new MFWrapper(
					grabberConfig["device"].toString("auto"),
					grabberConfig["width"].toInt(0),
					grabberConfig["height"].toInt(0),
					grabberConfig["fps"].toInt(15),
					grabberConfig["input"].toInt(-1),					
					parsePixelFormat(grabberConfig["pixelFormat"].toString("no-change")),
					_rootPath);
					
			// HDR stuff					
			if (!grabberConfig["hdrToneMapping"].toBool(false))	
			{
				_mfGrabber->setHdrToneMappingEnabled(0);
			}
			else
			{
				_mfGrabber->setHdrToneMappingEnabled(grabberConfig["hdrToneMappingMode"].toInt(1));				
			}
			setVideoModeHdr(_mfGrabber->getHdrToneMappingEnabled());
			// software frame skipping
			_mfGrabber->setFpsSoftwareDecimation(grabberConfig["fpsSoftwareDecimation"].toInt(1));
			_mfGrabber->setEncoding(grabberConfig["v4l2Encoding"].toString("NONE"));
			
			_mfGrabber->setSignalThreshold(
					grabberConfig["redSignalThreshold"].toDouble(0.0) / 100.0,
					grabberConfig["greenSignalThreshold"].toDouble(0.0) / 100.0,
					grabberConfig["blueSignalThreshold"].toDouble(0.0) / 100.0,
					grabberConfig["noSignalCounterThreshold"].toInt(50) );
					
			_mfGrabber->setCropping(
					grabberConfig["cropLeft"].toInt(0),
					grabberConfig["cropRight"].toInt(0),
					grabberConfig["cropTop"].toInt(0),
					grabberConfig["cropBottom"].toInt(0));

			_mfGrabber->setQFrameDecimation(grabberConfig["qFrame"].toBool(false));
			
			_mfGrabber->setBrightnessContrastSaturationHue(grabberConfig["hardware_brightness"].toInt(0), 
													grabberConfig["hardware_contrast"].toInt(0),
													grabberConfig["hardware_saturation"].toInt(0),
													grabberConfig["hardware_hue"].toInt(0));
			
			_mfGrabber->setSignalDetectionEnable(grabberConfig["signalDetection"].toBool(true));
			_mfGrabber->setSignalDetectionOffset(
					grabberConfig["sDHOffsetMin"].toDouble(0.25),
					grabberConfig["sDVOffsetMin"].toDouble(0.25),
					grabberConfig["sDHOffsetMax"].toDouble(0.75),
					grabberConfig["sDVOffsetMax"].toDouble(0.75));
			Debug(_log, "Media Foundation grabber created");

			// connect to HyperionDaemon signal
			connect(this, &HyperHdrDaemon::videoModeHdr, _mfGrabber, &MFWrapper::setHdrToneMappingEnabled);			
			connect(this, &HyperHdrDaemon::settingsChanged, _mfGrabber, &MFWrapper::handleSettingsUpdate);
			connect(_mfGrabber, &MFWrapper::HdrChanged, this, &HyperHdrDaemon::videoModeHdr);			
		}
#elif !defined(ENABLE_V4L2)
		Warning(_log, "The MF grabber can not be instantiated, because it has been left out from the build");		
#endif


#if defined(ENABLE_V4L2)
		if (_v4l2Grabber != nullptr)
			return;
		
		_v4l2Grabber = new V4L2Wrapper(
				grabberConfig["device"].toString("auto"),
				grabberConfig["width"].toInt(0),
				grabberConfig["height"].toInt(0),
				grabberConfig["fps"].toInt(15),
				grabberConfig["input"].toInt(-1),
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
		setVideoModeHdr(_v4l2Grabber->getHdrToneMappingEnabled());
		
		// software frame skipping
		_v4l2Grabber->setFpsSoftwareDecimation(grabberConfig["fpsSoftwareDecimation"].toInt(1));
		_v4l2Grabber->setEncoding(grabberConfig["v4l2Encoding"].toString("NONE"));
		
		_v4l2Grabber->setSignalThreshold(
				grabberConfig["redSignalThreshold"].toDouble(0.0) / 100.0,
				grabberConfig["greenSignalThreshold"].toDouble(0.0) / 100.0,
				grabberConfig["blueSignalThreshold"].toDouble(0.0) / 100.0,
				grabberConfig["noSignalCounterThreshold"].toInt(50) );
		_v4l2Grabber->setCropping(
				grabberConfig["cropLeft"].toInt(0),
				grabberConfig["cropRight"].toInt(0),
				grabberConfig["cropTop"].toInt(0),
				grabberConfig["cropBottom"].toInt(0));

		_v4l2Grabber->setQFrameDecimation(grabberConfig["qFrame"].toBool(false));
		
		_v4l2Grabber->setBrightnessContrastSaturationHue(grabberConfig["hardware_brightness"].toInt(0), 
													grabberConfig["hardware_contrast"].toInt(0),
													grabberConfig["hardware_saturation"].toInt(0),
													grabberConfig["hardware_hue"].toInt(0));
		
		_v4l2Grabber->setSignalDetectionEnable(grabberConfig["signalDetection"].toBool(true));
		_v4l2Grabber->setSignalDetectionOffset(
				grabberConfig["sDHOffsetMin"].toDouble(0.25),
				grabberConfig["sDVOffsetMin"].toDouble(0.25),
				grabberConfig["sDHOffsetMax"].toDouble(0.75),
				grabberConfig["sDVOffsetMax"].toDouble(0.75));
		Debug(_log, "V4L2 grabber created");

		// connect to HyperionDaemon signal
		connect(this, &HyperHdrDaemon::videoModeHdr, _v4l2Grabber, &V4L2Wrapper::setHdrToneMappingEnabled);		
		connect(this, &HyperHdrDaemon::settingsChanged, _v4l2Grabber, &V4L2Wrapper::handleSettingsUpdate);
		connect(_v4l2Grabber, &V4L2Wrapper::HdrChanged, this, &HyperHdrDaemon::videoModeHdr);
#elif !defined(ENABLE_WMF)	
		Warning(_log, "!The v4l2 grabber can not be instantiated, because it has been left out from the build");		
#endif
	}
}
