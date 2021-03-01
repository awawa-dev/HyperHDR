#pragma once

#include <QApplication>
#include <QObject>
#include <QJsonObject>

#ifdef ENABLE_V4L2
	#include <grabber/V4L2Wrapper.h>
#else
	typedef QObject V4L2Wrapper;
#endif

#ifdef ENABLE_WMF
	#include <grabber/MFWrapper.h>
#else
	typedef QObject MFWrapper;
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


class HyperHdrDaemon : public QObject
{
	Q_OBJECT

	friend SysTray;

public:
	HyperHdrDaemon(const QString& rootPath, QObject *parent, bool logLvlOverwrite, bool readonlyMode = false);
	~HyperHdrDaemon();

	///
	/// @brief Get webserver pointer (systray)
	///
	WebServer *getWebServerInstance() { return _webserver; }

	///
	/// @brief Get the current videoMode
	///
	int       getVideoModeHdr() const { return _currVideoModeHdr; };

	///
	/// @brief get the settings
	///
	QJsonDocument getSetting(settings::type type) const;

	void startNetworkServices();

	static HyperHdrDaemon* getInstance() { return daemon; }
	static HyperHdrDaemon* daemon;

public slots:
	void freeObjects();

signals:
	///////////////////////////////////////
	/// FROM HYPERIONDAEMON TO HYPERION ///
	///////////////////////////////////////

	///
	/// @brief After eval of setVideoMode this signal emits with a new one on change
	///
	void videoModeHdr(int hdr);

	///////////////////////////////////////
	/// FROM HYPERION TO HYPERIONDAEMON ///
	///////////////////////////////////////

	///
	/// @brief PIPE settings events from Hyperion class to HyperionDaemon components
	///
	void settingsChanged(settings::type type, const QJsonDocument& data);

	///
	/// @brief PIPE component state changes events from Hyperion class to HyperionDaemon components
	///
	void compStateChangeRequest(hyperhdr::Components component, bool enable);

private slots:
	///
	/// @brief Handle settings update from Hyperion Settingsmanager emit or this constructor
	/// @param type   settingyType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

	///
	/// @brief Listen for videoMode changes and emit videoMode in case of a change, update _currVideoMode
	/// @param mode  The requested video mode
	///
	void setVideoModeHdr(int hdr);

private:

	Logger*                    _log;
	HyperHdrIManager*          _instanceManager;
	AuthManager*               _authManager;
	BonjourBrowserWrapper*     _bonjourBrowserWrapper;
	NetOrigin*                 _netOrigin;
	WebServer*                 _webserver;
	WebServer*                 _sslWebserver;
	JsonServer*                _jsonServer;
	V4L2Wrapper*               _v4l2Grabber;
	MFWrapper*                 _mfGrabber;
	SSDPHandler*               _ssdp;
	FlatBufferServer*          _flatBufferServer;
	ProtoServer*               _protoServer;

    #if defined(ENABLE_SOUNDCAPWINDOWS)
        SoundCapWindows*		_snd;
    #elif defined(ENABLE_SOUNDCAPLINUX)
        SoundCapLinux*			_snd;
    #endif

	unsigned                   _grabber_width;
	unsigned                   _grabber_height;
	unsigned                   _grabber_frequency;
	unsigned                   _grabber_cropLeft;
	unsigned                   _grabber_cropRight;
	unsigned                   _grabber_cropTop;
	unsigned                   _grabber_cropBottom;
	int                        _grabber_ge2d_mode;
	QString                    _grabber_device;

	QString                    _prevType;

	int                        _currVideoModeHdr;
	SettingsManager*           _settingsManager;
	
	// application root path
	QString _rootPath;
};
