#pragma once

#include <HyperhdrConfig.h>

#ifndef PCH_ENABLED	
	#include <QObject>
	#include <QJsonObject>
	#include <QStringList>
#endif

#include <QCoreApplication>

#ifdef ENABLE_V4L2
	#include <grabber/linux/v4l2/V4L2Wrapper.h>
#else
	typedef QObject V4L2Wrapper;
#endif

#ifdef ENABLE_MF
	#include <grabber/windows/MF/MFWrapper.h>
#else
	typedef QObject MFWrapper;
#endif

#ifdef ENABLE_AVF
	#include <grabber/osx/AVF/AVFWrapper.h>
#else
	typedef QObject AVFWrapper;
#endif

#ifdef ENABLE_DX
#include <grabber/windows/DX/DxWrapper.h>
#else
	typedef QObject DxWrapper;
#endif

#ifdef ENABLE_X11
#include <grabber/linux/X11/X11Wrapper.h>
#else
	typedef QObject X11Wrapper;
#endif

#ifdef ENABLE_FRAMEBUFFER
#include <grabber/linux/framebuffer/FrameBufWrapper.h>
#else
	typedef QObject FrameBufWrapper;
#endif

#ifdef ENABLE_PIPEWIRE
#include <grabber/linux/pipewire/PipewireWrapper.h>
#else
	typedef QObject PipewireWrapper;
#endif

#ifdef ENABLE_MQTT
#include <mqtt/mqtt.h>
#else
	typedef QObject mqtt;
#endif


#ifdef ENABLE_MAC_SYSTEM
#include <grabber/osx/macOS/macOsWrapper.h>
#else
	typedef QObject macOsWrapper;
#endif

#ifdef ENABLE_BONJOUR
#include <bonjour/DiscoveryWrapper.h>
#else
	typedef QObject DiscoveryWrapper;
#endif

#ifdef ENABLE_CEC
	#include <cec/WrapperCEC.h>
#else
	typedef QObject WrapperCEC;
#endif

#if defined(ENABLE_SOUNDCAPWINDOWS)
	#include <sound-capture/windows/SoundCaptureWindows.h>
#elif defined(ENABLE_SOUNDCAPLINUX)
	#include <sound-capture/linux/SoundCaptureLinux.h>
#elif defined(ENABLE_SOUNDCAPMACOS)
	#include <sound-capture/macos/SoundCaptureMacOS.h>
#else
	class SoundCapture : public QObject {};
#endif

#include <utils/Logger.h>

// settings management
#include <utils/settings.h>
#include <utils/Components.h>
#include <memory>

class HyperHdrManager;
class PerformanceCounters;
class SysTray;
class JsonServer;
class WebServer;
class InstanceConfig;
class SSDPHandler;
class FlatBufferServer;
class ProtoServer;
class AccessManager;
class NetOrigin;
class VideoThread;
class GrabberHelper;
class QApplication;

#if defined(_WIN32) && defined(ENABLE_POWER_MANAGEMENT)
	#include <suspend-handler/SuspendHandlerWindows.h>
#elif defined(__APPLE__) && defined(ENABLE_POWER_MANAGEMENT)
	#include <suspend-handler/SuspendHandlerMacOS.h>
#elif defined(__linux__) && defined(ENABLE_POWER_MANAGEMENT)
	#include <suspend-handler/SuspendHandlerLinux.h>
#else
	#include <suspend-handler/SuspendHandlerDummy.h>	
#endif

namespace hyperhdr { enum class InstanceState; }

class HyperHdrDaemon : public QObject
{
	Q_OBJECT


public:
	HyperHdrDaemon(const QString& rootPath, QCoreApplication* parent, bool logLvlOverwrite, bool readonlyMode = false, QStringList params = QStringList(), bool isGuiApp = true);
	~HyperHdrDaemon();

	QJsonDocument getSetting(settings::type type) const;	

public slots:
	void freeObjects();
	quint16 getWebPort();

public slots:
	void instanceStateChangedHandler(hyperhdr::InstanceState state, quint8 instance, const QString& name = QString());
	void settingsChangedHandler(settings::type type, const QJsonDocument& config);
	void getInstanceManager(std::shared_ptr<HyperHdrManager>& retVal);
	void getAccessManager(std::shared_ptr<AccessManager>& retVal);
	void getSoundCapture(std::shared_ptr<SoundCapture>& retVal);
	void getSystemGrabber(std::shared_ptr<GrabberHelper>& systemGrabber);
	void getVideoGrabber(std::shared_ptr<GrabberHelper>& videoGrabber);
	void getPerformanceCounters(std::shared_ptr<PerformanceCounters>& performanceCounters);
	void getDiscoveryWrapper(std::shared_ptr<DiscoveryWrapper>& discoveryWrapper);

private:
	void startNetworkServices();
	template<typename T> void createVideoGrabberHelper(QJsonDocument config, QString deviceName, QString rootPath);
	void createSoftwareGrabberHelper(QJsonDocument config, QString deviceName, QString rootPath);

	Logger*					_log;
	std::shared_ptr<HyperHdrManager> _instanceManager;
	std::shared_ptr<AccessManager> _accessManager;
	std::shared_ptr<NetOrigin> _netOrigin;
	std::shared_ptr<GrabberHelper> _videoGrabber;
	std::shared_ptr<GrabberHelper> _systemGrabber;
	std::shared_ptr<SoundCapture>  _soundCapture;
	std::shared_ptr<PerformanceCounters> _performanceCounters;
	std::shared_ptr<DiscoveryWrapper> _discoveryWrapper;
	std::unique_ptr<QThread, std::function<void(QThread*)>> _flatProtoBuffersThread;
	std::unique_ptr<QThread, std::function<void(QThread*)>> _networkThread;
	std::unique_ptr<QThread, std::function<void (QThread*)>> _mqttThread;
	std::unique_ptr<SuspendHandler> _suspendHandler;
	std::unique_ptr<WrapperCEC>	_wrapperCEC;
	std::unique_ptr<InstanceConfig> _instanceZeroConfig;

	QString				_rootPath;
	bool				_readonlyMode;
	QStringList			_params;
	bool				_isGuiApp;
	bool				_disableOnStart;
};

