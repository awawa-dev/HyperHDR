#pragma once

#include <QApplication>
#include <QObject>
#include <QJsonObject>
#include <QStringList>

#ifdef ENABLE_V4L2
	#include <grabber/V4L2Wrapper.h>
#else
	typedef QObject V4L2Wrapper;
#endif

#ifdef ENABLE_MF
	#include <grabber/MFWrapper.h>
#else
	typedef QObject MFWrapper;
#endif

#ifdef ENABLE_AVF
	#include <grabber/AVFWrapper.h>
#else
	typedef QObject AVFWrapper;
#endif

#ifdef ENABLE_DX
#include <grabber/DxWrapper.h>
#else
	typedef QObject DxWrapper;
#endif

#ifdef ENABLE_X11
#include <grabber/X11Wrapper.h>
#else
	typedef QObject X11Wrapper;
#endif

#ifdef ENABLE_FRAMEBUFFER
#include <grabber/FrameBufWrapper.h>
#else
	typedef QObject FrameBufWrapper;
#endif

#ifdef ENABLE_PIPEWIRE
#include <grabber/PipewireWrapper.h>
#else
	typedef QObject PipewireWrapper;
#endif


#ifdef ENABLE_MAC_SYSTEM
#include <grabber/macOsWrapper.h>
#else
	typedef QObject macOsWrapper;
#endif

#ifdef ENABLE_CEC
#include <cec/cecHandler.h>
#else
	typedef QObject cecHandler;
#endif

#include <utils/Logger.h>

// settings management
#include <utils/settings.h>
#include <utils/Components.h>

class HyperHdrIManager;
class SysTray;
class JsonServer;
class BonjourBrowserWrapper;
class WebServer;
class SettingsManager;
class SSDPHandler;
class FlatBufferServer;
class ProtoServer;
class AuthManager;
class NetOrigin;
#ifdef ENABLE_SOUNDCAPWINDOWS
	class SoundCapWindows;
#endif
#ifdef ENABLE_SOUNDCAPLINUX
	class SoundCapLinux;
#endif
#ifdef ENABLE_SOUNDCAPMACOS
	class SoundCapMacOS;
#endif

#if defined(_WIN32)
	#include "WinSuspend.h"
#elif defined(__linux__) && defined(HYPERHDR_HAVE_DBUS)
	#include "LinuxSuspend.h"
#else
	typedef QObject SuspendHandler;
#endif

enum class InstanceState;


class HyperHdrDaemon : public QObject
{
	Q_OBJECT

		friend SysTray;

public:
	HyperHdrDaemon(const QString& rootPath, QObject* parent, bool logLvlOverwrite, bool readonlyMode = false, QStringList params = QStringList());
	~HyperHdrDaemon();

	///
	/// @brief Get webserver pointer (systray)
	///
	WebServer* getWebServerInstance() { return _webserver; }

	///
	/// @brief get the settings
	///
	QJsonDocument getSetting(settings::type type) const;

	void startNetworkServices();

	static HyperHdrDaemon* getInstance() { return daemon; }
	static HyperHdrDaemon* daemon;



public slots:
	void freeObjects();
	void enableCEC(bool enabled, QString info);
	void keyPressedCEC(int keyCode);

signals:

	///////////////////////////////////////
	/// FROM HYPERHDR TO HYPERHDRDAEMON ///
	///////////////////////////////////////

	///
	/// @brief PIPE settings events from HyperHDR class to HyperHDRDaemon components
	///
	void settingsChanged(settings::type type, const QJsonDocument& data);

	///
	/// @brief PIPE component state changes events from HyperHDR class to HyperHDRDaemon components
	///
	void compStateChangeRequest(hyperhdr::Components component, bool enable);

private slots:
	///
	/// @brief Handle settings update from HyperHDR Settingsmanager emit or this constructor
	/// @param type   settingyType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
public slots:
	void instanceStateChanged(InstanceState state, quint8 instance, const QString& name = QString());
	void handleSettingsUpdateGlobal(settings::type type, const QJsonDocument& config);
private:
	void loadCEC();
	void unloadCEC();
	void updateCEC();

	Logger*					_log;
	HyperHdrIManager*		_instanceManager;
	AuthManager*			_authManager;
	BonjourBrowserWrapper*	_bonjourBrowserWrapper;
	NetOrigin*				_netOrigin;
	WebServer*				_webserver;
	WebServer*				_sslWebserver;
	JsonServer*				_jsonServer;
	V4L2Wrapper*			_v4l2Grabber;
	MFWrapper*				_mfGrabber;
	AVFWrapper*				_avfGrabber;
	macOsWrapper*			_macGrabber;
	DxWrapper*				_dxGrabber;
	X11Wrapper*				_x11Grabber;
	FrameBufWrapper*		_fbGrabber;
	PipewireWrapper*		_pipewireGrabber;	
	cecHandler*				_cecHandler;
	SSDPHandler*			_ssdp;
	FlatBufferServer*		_flatBufferServer;

#if defined(ENABLE_PROTOBUF)
	ProtoServer* _protoServer;
#endif

#if defined(ENABLE_SOUNDCAPWINDOWS)
	SoundCapWindows* _snd;
#elif defined(ENABLE_SOUNDCAPLINUX)
	SoundCapLinux* _snd;
#elif defined(ENABLE_SOUNDCAPMACOS)
	SoundCapMacOS* _snd;
#endif

	std::unique_ptr<SuspendHandler>	_suspendHandler;

	SettingsManager* _settingsManager;

	// application root path
	QString			_rootPath;
	QStringList		_params;
};
