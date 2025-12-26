/* PipewireHandler.cpp
* 
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <QObject>
#include <QFlags>
#include <QString>
#include <QVariantMap>
#include <QDebug>
#include <QUuid>
#include <QStringList>


#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>
	

#include <grabber/linux/pipewire/smartPipewire.h>
#include <grabber/linux/pipewire/PipewireHandler.h>
#include <grabber/linux/pipewire/ScreenCastProxy.h>
#include <utils/Macros.h>

#ifndef DRM_FORMAT_MOD_INVALID
	#define DRM_FORMAT_MOD_INVALID ((1ULL<<56) - 1)
#endif

using namespace sdbus;
using namespace org::freedesktop::portal;

class ScreenCastProxy final : public sdbus::ProxyInterfaces<org::freedesktop::portal::ScreenCast_proxy>
{
public:
    ScreenCastProxy(sdbus::IConnection& connection, sdbus::ServiceName destination, sdbus::ObjectPath path)
    : ProxyInterfaces(connection, std::move(destination), std::move(path))
    {
        registerProxy();
    }

    ~ScreenCastProxy()
    {
        unregisterProxy();
    }
};

// Pipewire screen grabber using Portal access interface

Q_DECLARE_METATYPE(QList<PipewireHandler::PipewireStructure>);
Q_DECLARE_METATYPE(PipewireHandler::PipewireStructure);
Q_DECLARE_METATYPE(pw_stream_state);
Q_DECLARE_METATYPE(uint32_t);

constexpr const char* DESKTOP_SERVICE = "org.freedesktop.portal.Desktop";
constexpr const char* DESKTOP_PATH = "/org/freedesktop/portal/desktop";
constexpr const char* PORTAL_REQUEST = "org.freedesktop.portal.Request";
constexpr const char* PORTAL_SESSION = "org.freedesktop.portal.Session";
constexpr const char* PORTAL_RESPONSE = "Response";

constexpr const char* REQUEST_TEMPLATE = "/org/freedesktop/portal/desktop/request/%1/%2";

constexpr const int DEFAULT_UPDATE_NUMBER = 4;

PipewireHandler::PipewireHandler() :
									_sessionHandle(""), _restorationToken(""), _errorMessage(""), _portalStatus(false),
									_isError(false), _version(-1), _streamNodeId(0),
									_sender(""), _replySessionPath(""), _sourceReplyPath(""), _startReplyPath(""),
									_pwMainThreadLoop(nullptr), _pwNewContext(nullptr), _pwContextConnection(nullptr), _pwStream(nullptr),
									_targetMaxSize(512), _frameWidth(0),_frameHeight(0),_frameOrderRgb(false), _requestedFPS(10), _incomingFrame(nullptr),
									_infoUpdate(DEFAULT_UPDATE_NUMBER), _initEGL(false), _enableEGL(true), _libEglHandle(nullptr), _libGlHandle(nullptr),
									_frameDrmFormat(DRM_FORMAT_MOD_INVALID), _frameDrmModifier(DRM_FORMAT_MOD_INVALID), _image{}
{
	_pwStreamListener = {};
	_pwCoreListener = {};

	qRegisterMetaType<uint32_t>();
	qRegisterMetaType<pw_stream_state>();

	connect(this, &PipewireHandler::onParamsChangedSignal,	this, &PipewireHandler::onParamsChanged);
	connect(this, &PipewireHandler::onStateChangedSignal,	this, &PipewireHandler::onStateChanged);
	connect(this, &PipewireHandler::onProcessFrameSignal,	this, &PipewireHandler::onProcessFrame);
	connect(this, &PipewireHandler::onCoreErrorSignal,		this, &PipewireHandler::onCoreError);
}

QString PipewireHandler::getToken()
{
	if (_portalStatus)
		return _restorationToken;
	else
		return "";
}


QString PipewireHandler::getError()
{
	return _errorMessage;
}

PipewireHandler::~PipewireHandler()
{
	closeSession();

	if (_libEglHandle != nullptr)
	{
		dlclose(_libEglHandle);
		_libEglHandle = nullptr;
	}

	if (_libGlHandle != nullptr)
	{
		dlclose(_libGlHandle);
		_libGlHandle = nullptr;
	}
}

void PipewireHandler::closeSession()
{
	if (_pwMainThreadLoop != nullptr)
	{
		pw_thread_loop_stop(_pwMainThreadLoop);		
	}

	if (_pwStream != nullptr)
	{		
		pw_stream_destroy(_pwStream);
		_pwStream = nullptr;
	}

	if (_pwContextConnection != nullptr)
	{
		pw_core_disconnect(_pwContextConnection);
		_pwContextConnection = nullptr;
	}

	if (_pwNewContext != nullptr)
	{
		pw_context_destroy(_pwNewContext);
		_pwNewContext = nullptr;
	}

	if (_pwMainThreadLoop != nullptr)
	{
		pw_thread_loop_destroy(_pwMainThreadLoop);
		_pwMainThreadLoop = nullptr;
	}

	if (!_sessionHandle.isEmpty() && _dbusConnection != nullptr)
	{
		try
		{
			sdbus::ServiceName destination{ DESKTOP_SERVICE };
			sdbus::ObjectPath objectPath{_sessionHandle.toStdString()};
			auto sessionProxy = sdbus::createProxy(*_dbusConnection, std::move(destination), std::move(objectPath));
			auto call = sessionProxy->createMethodCall(sdbus::InterfaceName{ PORTAL_SESSION }, sdbus::MethodName {"Close"});
			auto reply = sessionProxy->callMethod(call);
		}
		catch (std::exception& e)
		{
			qWarning().nospace() << "Pipewire: could not close session: " << e.what();
		}
	}

	_startReplyPath = "";
	_sourceReplyPath = "";
	_replySessionPath = "";
	_sessionHandle = "";
	
	_screenCastProxy = nullptr;
	_createSessionProxy = nullptr;
	_selectSourceProxy = nullptr;
	_startProxy = nullptr;
	_dbusConnection = nullptr;

	_pwStreamListener = {};
	_pwCoreListener = {};	
	_portalStatus = false;
	_isError = false;
	_errorMessage = "";
	_streamNodeId = 0;
	_frameWidth = 0;
	_frameHeight = 0;
	_frameOrderRgb = false;
	_requestedFPS = 10;
	_incomingFrame = nullptr;
	_infoUpdate = DEFAULT_UPDATE_NUMBER;
	_frameDrmFormat = DRM_FORMAT_MOD_INVALID;
	_frameDrmModifier = DRM_FORMAT_MOD_INVALID;

#ifdef ENABLE_PIPEWIRE_EGL
	if (contextEgl != EGL_NO_CONTEXT && displayEgl != EGL_NO_DISPLAY && eglGetCurrentContext() != contextEgl)
	{
		eglMakeCurrent(displayEgl, EGL_NO_SURFACE, EGL_NO_SURFACE, contextEgl);
	}

	if (_eglTexture != 0)
	{
		glDeleteTextures(1, &_eglTexture);
		_eglTexture = 0;
		qDebug() << "Pipewire EGL: target texture deleted";
	}

	if (_eglScratchTex != 0)
	{
		glDeleteTextures(1, &_eglScratchTex);
		_eglScratchTex = 0;
		qDebug() << "Pipewire EGL: scratch texture deleted";
	}

	if (_eglFbos[0] != 0 || _eglFbos[1] != 0)
	{
		glDeleteFramebuffers(static_cast<GLsizei>(_eglFbos.size()), _eglFbos.data());
		_eglFbos.fill(0);
		qDebug() << "Pipewire EGL: eglFbos deleted";
	}

	if (contextEgl != EGL_NO_CONTEXT)
	{
		auto rel = eglMakeCurrent(displayEgl, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		qDebug().nospace() << "PipewireEGL: releasing EGL context => " << rel;

		auto res = eglDestroyContext(displayEgl, contextEgl);
		qDebug().nospace() << "PipewireEGL: destroying EGL context => " << res;
		contextEgl = EGL_NO_CONTEXT;
	}

	if (displayEgl != EGL_NO_DISPLAY)
	{
		auto res = eglTerminate(displayEgl);
		qDebug().nospace() << "PipewireEGL: terminate the display => " << res;
		displayEgl = EGL_NO_DISPLAY;
	}

	_initEGL = false;

	for (supportedDmaFormat& supVal : _supportedDmaFormatsList)
		supVal.hasDma = false;

#endif

	releaseWorkingFrame();

	_image = {};
	createMemory(0);	

	qDebug().nospace() << "Pipewire: driver is closed now";
}

void PipewireHandler::releaseWorkingFrame()
{
}

QString PipewireHandler::getSessionToken()
{
	return QString("hyperhdr_s%1").arg(QUuid::createUuid().toString(QUuid::Id128));
}

QString PipewireHandler::getRequestToken()
{
	return QString("hyperhdr_r%1").arg(QUuid::createUuid().toString(QUuid::Id128));
}

void PipewireHandler::reportError(const QString& input)
{
	_isError = true;
	_errorMessage = input;
	qCritical().nospace() << qPrintable(input);
}

bool PipewireHandler::hasError()
{
	return _isError;
}

int PipewireHandler::getVersion()
{
	return _version;
}

int PipewireHandler::readVersion()
{
	int version = -1;

	try
	{
		auto bus = sdbus::createSessionBusConnection();
		auto proxy = std::make_unique<ScreenCastProxy>(*bus, ServiceName{ DESKTOP_SERVICE }, ObjectPath{ DESKTOP_PATH });

		version = proxy->version();
	}
	catch (std::exception& e)
	{
		qCritical().nospace() << "Pipewire: could not read Portal ScreenCast version";
		version = -1;
	}

	return version;
}

void PipewireHandler::startSession(QString restorationToken, uint32_t requestedFPS, bool enableEGL, int targetMaxSize)
{
	qDebug().nospace() << "Pipewire: initialization invoked. Cleaning up first...";

	closeSession();

	_enableEGL = enableEGL;
	_targetMaxSize = targetMaxSize;

	#ifdef ENABLE_PIPEWIRE_EGL
		qDebug().nospace() << "Pipewire: support for EGL is " << ((_enableEGL) ? "enabled" : "disabled");
	#else
		qDebug().nospace() << "Pipewire: support for EGL is DISABLED (in the build)";
	#endif

	if (requestedFPS < 1 || requestedFPS > 60)
	{
		reportError("Pipewire: invalid capture rate.");
		return;
	}

	qDebug().nospace() << "Pipewire: targetMaxSize = " << _targetMaxSize;

	try
	{
		_dbusConnection = sdbus::createSessionBusConnection();
		_screenCastProxy = std::make_unique<ScreenCastProxy>(*_dbusConnection, ServiceName{ DESKTOP_SERVICE }, ObjectPath{ DESKTOP_PATH });
		_dbusConnection->enterEventLoopAsync();

		_version = _screenCastProxy->version();
	}
	catch(std::exception& e)
	{
		qCritical().nospace() << "Pipewire: could not read Portal ScreenCast version";
		_version = -1;
	}

	_restorationToken = QString("%1").arg(restorationToken);

	_image.version = _version;

	if (_version < 0)
	{
		reportError("Pipewire: Couldn't read Portal.ScreenCast protocol version. Probably Portal is not installed.");
		return;
	}

	_requestedFPS = requestedFPS;

	_sender = QString("%1").arg(QString::fromStdString(_dbusConnection->getUniqueName())).replace('.','_');
	if (_sender.length() > 0 && _sender[0] == ':')
		_sender = _sender.right(_sender.length()-1);

	qDebug().nospace() << "Sender: " << qPrintable(_sender);

	QString requestUUID = getRequestToken();

	_replySessionPath = QString(REQUEST_TEMPLATE).arg(_sender).arg(requestUUID);


	std::map<std::string, sdbus::Variant> createSessionParams{
		{"session_handle_token", sdbus::Variant(getSessionToken().toStdString())},
		{"handle_token", sdbus::Variant(requestUUID.toStdString())}
	};
    try
    {
		auto responseSignalHandler = [&] (uint32_t resultCode, std::map<std::string, sdbus::Variant> results)
		{
			auto sessionHandleIter = results.find("session_handle");
			if (sessionHandleIter == results.end())
			{
				qDebug().nospace() << "Create session didnt return a handle";
			}
			else
			{
				QString session = QString::fromStdString(sessionHandleIter->second.get<std::string>());
				QUEUE_CALL_2(this, createSessionResponse, uint, resultCode, QString, session );
			}
		};

		_createSessionProxy = sdbus::createProxy(*_dbusConnection, sdbus::ServiceName{""}, sdbus::ObjectPath{_replySessionPath.toStdString()});

		_createSessionProxy->uponSignal(SignalName{ PORTAL_RESPONSE }).onInterface(InterfaceName{ PORTAL_REQUEST }).call(responseSignalHandler);

        _screenCastProxy->CreateSession(createSessionParams);

    }
	catch(std::exception& ex)
    {
        reportError(QString("Pipewire: Failed to create session: %1").arg(QString::fromLocal8Bit(ex.what())));
    }
	qDebug().nospace() << "Requested FPS: " << _requestedFPS;
	qDebug().nospace() << "Pipewire: CreateSession finished";
}


void PipewireHandler::createSessionResponse(uint response, QString session)
{
	qDebug().nospace() << "Pipewire: Got response from portal CreateSession";

	if (response != 0)
	{
		reportError(QString("Pipewire: Failed to create session: %1").arg(response));
		return;
	}

	QString requestUUID = getRequestToken();

	_sessionHandle = session;

	_sourceReplyPath = QString(REQUEST_TEMPLATE).arg(_sender).arg(requestUUID);

	std::map<std::string, sdbus::Variant> selectSourceParams{
		{ "multiple", sdbus::Variant(false)},
		{ "types", sdbus::Variant((uint)1)},
		{ "cursor_mode", sdbus::Variant((uint)1) },
		{ "handle_token", sdbus::Variant(requestUUID.toStdString()) },
		{ "persist_mode", sdbus::Variant((uint)2) } };

	if (!_restorationToken.isEmpty())
	{
		selectSourceParams["restore_token"] = sdbus::Variant(_restorationToken.toStdString());
		qDebug().nospace() << "Pipewire: Has restoration token: " << qPrintable(QString(_restorationToken).right(12));
	}

	try
	{
		auto responseSignalHandler = [&] (uint32_t resultCode, std::map<std::string, sdbus::Variant> results)
		{
			QUEUE_CALL_1(this, selectSourcesResponse, uint, resultCode);
		};

		_selectSourceProxy = sdbus::createProxy(*_dbusConnection, sdbus::ServiceName{""}, sdbus::ObjectPath{_sourceReplyPath.toStdString()});
		_selectSourceProxy->uponSignal(SignalName{ PORTAL_RESPONSE }).onInterface(InterfaceName{ PORTAL_REQUEST }).call(responseSignalHandler);

		_screenCastProxy->SelectSources(ObjectPath{_sessionHandle.toStdString()}, selectSourceParams);

	}
	catch(std::exception& ex)
	{
		reportError(QString("Pipewire: Failed to select a source: %1").arg(QString::fromLocal8Bit(ex.what())));
	}

	qDebug().nospace() << "Pipewire: SelectSources finished";
}


void PipewireHandler::selectSourcesResponse(uint response)
{
	qDebug().nospace() << "Pipewire: Got response from portal SelectSources";

	if (response != 0) {
		reportError(QString("Pipewire: Failed to select sources: %1").arg(response));
		return;
	}

	QString requestUUID = getRequestToken();

	_startReplyPath = QString(REQUEST_TEMPLATE).arg(_sender).arg(requestUUID);

	std::map<std::string, sdbus::Variant> startParams{{ "handle_token", sdbus::Variant(requestUUID.toStdString()) }};

	try
	{
		auto responseSignalHandler = [&] (uint32_t resultCode, std::map<std::string, sdbus::Variant> results)
		{
			try
			{
				QString restoreHandle;
				auto restoreHandleIter = results.find("restore_token");
				if (restoreHandleIter == results.end())
				{
					qWarning().nospace() << "Start session didnt return a restoration handle";
				}
				else
				{
					restoreHandle = QString::fromStdString(restoreHandleIter->second.get<std::string>());
				}

				auto streamsIter = results.find("streams");
				if (streamsIter == results.end())
				{
					qCritical().nospace() << "Start session didnt return streams";
				}
				else
				{
					std::vector<sdbus::Struct<uint32_t, std::map<std::string, sdbus::Variant>>> streams =
						streamsIter->second.get<std::vector<sdbus::Struct<uint32_t, std::map<std::string, sdbus::Variant>>>>();
					if (streams.empty())
					{
						qWarning().nospace() << "Start session didnt return any stream";
					}

					auto stream = streams[0];
					int nodeStreamWidth = 0;
					int nodeStreamHeight = 0;
					uint32_t nodeId = stream.get<0>();
					auto nodeStruct = stream.get<1>();

					auto sizeIter = nodeStruct.find("size");
					if (sizeIter == nodeStruct.end())
					{
						qWarning().nospace() << "Could not read stream size";
					}
					else
					{
						auto dim = sizeIter->second.get<sdbus::Struct<int32_t, int32_t>>();
						nodeStreamWidth = dim.get<0>();
						nodeStreamHeight = dim.get<1>();
					}

					QUEUE_CALL_5(this, startResponse, uint, resultCode, QString, restoreHandle, uint32_t, nodeId, int, nodeStreamWidth, int, nodeStreamHeight);

				}
			}
			catch (std::exception& ex)
			{
				reportError(QString("Pipewire: Failed to parse start parameters: %1").arg(QString::fromLocal8Bit(ex.what())));
			}
		};

		_startProxy = sdbus::createProxy(*_dbusConnection, sdbus::ServiceName{""}, sdbus::ObjectPath{_startReplyPath.toStdString()});
		_startProxy->uponSignal(SignalName{ PORTAL_RESPONSE }).onInterface(InterfaceName{ PORTAL_REQUEST }).call(responseSignalHandler);

		_screenCastProxy->Start( ObjectPath{_sessionHandle.toStdString()}, "", startParams);

	}
	catch(std::exception& ex)
	{
		reportError(QString("Pipewire: Failed to select a source: %1").arg(QString::fromLocal8Bit(ex.what())));
	}

	
	qDebug().nospace() << "Pipewire: Start finished";
}

void PipewireHandler::startResponse(uint response, QString restoreHandle, uint32_t nodeId, int nodeStreamWidth, int nodeStreamHeight)
{
	qDebug().nospace() << "Pipewire: Got response from portal Start";

	if (response != 0)
	{		
		reportError(QString("Pipewire: Failed to start or cancel dialog: %1").arg(response));
		_sessionHandle = "";
		return;
	}

	if (!restoreHandle.isEmpty())
	{
		_restorationToken = restoreHandle;
		qDebug().nospace() << "Received restoration token: " << qPrintable(QString(_restorationToken).right(12));
	}
	else
		qDebug().nospace() << "No restoration token (portal protocol version 4 required and must be implemented by the backend GNOME/KDE... etc)";


	_streamNodeId = nodeId;
	_frameWidth = nodeStreamWidth;
	_frameHeight = nodeStreamHeight;
	_portalStatus = true;
	
	//------------------------------------------------------------------------------------
	qDebug().nospace() << "Connecting to Pipewire interface for stream: " << _frameWidth << " x " << _frameHeight;

	pw_init(nullptr, nullptr);
	
	if ( nullptr == (_pwMainThreadLoop = pw_thread_loop_new("pipewire-hyperhdr-loop", nullptr)))
	{
		reportError("Pipewire: failed to create new Pipewire thread loop");
		return;
	}

	pw_thread_loop_lock(_pwMainThreadLoop);

	if ( nullptr == (_pwNewContext = pw_context_new(pw_thread_loop_get_loop(_pwMainThreadLoop), nullptr, 0)))
	{
		reportError("Pipewire: failed to create new Pipewire context");
		return;
	}
	
	if ( nullptr == (_pwContextConnection = pw_context_connect(_pwNewContext, nullptr, 0)))
	{
		reportError("Pipewire: could not connect to the Pipewire context");
		return;
	}

	if ( nullptr == (_pwStream = createCapturingStream()))
	{
		reportError("Pipewire: failed to create new receiving Pipewire stream");
		return;
	}

	if (pw_thread_loop_start(_pwMainThreadLoop) < 0)
	{
		reportError("Pipewire: could not start main Pipewire loop");
		return;
	}

	pw_thread_loop_unlock(_pwMainThreadLoop);
}


void PipewireHandler::onStateChanged(pw_stream_state old, pw_stream_state state, const char* error)
{	
	if (state == PW_STREAM_STATE_ERROR)
	{
		reportError(QString("Pipewire: stream reports error '%1'").arg(error));
		return;
	}
	else if (old == PW_STREAM_STATE_STREAMING && state != old)
	{
		reportError(QString("Pipewire: stream has stopped %1").arg(error));
	}

	if (state == PW_STREAM_STATE_UNCONNECTED) qDebug().nospace() << "Pipewire: state UNCONNECTED (" << (int)state << ", " << (int)old << ")";
	else if (state == PW_STREAM_STATE_CONNECTING) qDebug().nospace() << "Pipewire: state CONNECTING (" << (int)state << ", " << (int)old << ")";
	else if (state == PW_STREAM_STATE_PAUSED) qDebug().nospace() << "Pipewire: state PAUSED (" << (int)state << ", " << (int)old << ")";
	else if (state == PW_STREAM_STATE_STREAMING) qDebug().nospace() << "Pipewire: state STREAMING (" << (int)state << ", " << (int)old << ")";
	else qDebug().nospace() << "Pipewire: state UNKNOWN (" << (int)state << ", " << (int)old << ")";
}

void PipewireHandler::onParamsChanged(uint32_t id, const struct spa_pod* param)
{
	bool hasDma = false;
	struct spa_video_info format {};

	qDebug().nospace() << "Pipewire: got new video format selected";

	if (param == nullptr || id != SPA_PARAM_Format)
	{
		qDebug().nospace() << "Pipewire: not a SPA_PARAM_Format or empty. Got: " << id << ". Params are: " << ((param == nullptr) ? "empty" : "not empty");
		return;
	}

	if (spa_format_parse(param, &format.media_type, &format.media_subtype) < 0)
	{
		qDebug().nospace() << "Pipewire: not a media/submedia type";
		return;
	}

	if (format.media_type != SPA_MEDIA_TYPE_video || format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
	{
		qDebug().nospace() << "Pipewire: not a video or raw type";
		return;
	}

	if (spa_format_video_raw_parse(param, &format.info.raw) < 0)
	{
		qDebug().nospace() << "Pipewire: cant parse raw info";
		return;
	}

	_frameWidth = format.info.raw.size.width;
	_frameHeight = format.info.raw.size.height;
	_frameDrmModifier = format.info.raw.modifier;
	_frameOrderRgb = (format.info.raw.format == SPA_VIDEO_FORMAT_RGBx || format.info.raw.format == SPA_VIDEO_FORMAT_RGBA);

#ifdef ENABLE_PIPEWIRE_EGL
	hasDma = true;

	for (const supportedDmaFormat& val : _supportedDmaFormatsList)
		if (val.spaFormat == format.info.raw.format)
		{
			_frameDrmFormat = val.drmFormat;
			break;
		}
#endif

	qDebug().nospace() << "Pipewire: video format = " << format.info.raw.format << " (" << spa_debug_type_find_name(spa_type_video_format, format.info.raw.format) << ")";
	qDebug().nospace() << "Pipewire: video size = " << _frameWidth << "x" << _frameHeight << " (RGB order = " << ((_frameOrderRgb) ? "true" : "false") << ")";
	qDebug().nospace() << "Pipewire: framerate = " << format.info.raw.framerate.num << "/" << format.info.raw.framerate.denom;

	uint8_t updateBuffer[1536];
	struct spa_pod_builder updateBufferBuilder = SPA_POD_BUILDER_INIT(updateBuffer, sizeof(updateBuffer));
	const struct spa_pod* updatedParams[2];
	const uint32_t stride = SPA_ROUND_UP_N(_frameWidth * 4, 4);
	const int32_t size = _frameHeight * stride;

	const auto bufferTypes = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier)
		? (1 << SPA_DATA_DmaBuf) | (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr)
		: (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr);

	hasDma = hasDma && (bufferTypes & (1 << SPA_DATA_DmaBuf));

	// display capabilities
	if (hasDma)
	{
		qDebug().nospace() << "Pipewire: DMA buffer available. Format: " << fourCCtoString(_frameDrmFormat) << ". Modifier: " << fourCCtoString(_frameDrmModifier);
	}
	if (bufferTypes & (1 << SPA_DATA_MemFd))
	{
		qDebug().nospace() << "Pipewire: MemFD buffer available";
	}
	if (bufferTypes & (1 << SPA_DATA_MemPtr))
	{
		qDebug().nospace() << "Pipewire: MemPTR buffer available";
	}

	updatedParams[0] = (struct spa_pod*)spa_pod_builder_add_object(&updateBufferBuilder,
								SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
								SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
								SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header)));

	if (hasDma)
	{
		updatedParams[1] = reinterpret_cast<spa_pod*> (spa_pod_builder_add_object(&updateBufferBuilder,
								SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
								SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(1, 1, 8),
								SPA_PARAM_BUFFERS_size, SPA_POD_Int(size),
								SPA_PARAM_BUFFERS_stride, SPA_POD_CHOICE_RANGE_Int(stride, stride, INT32_MAX),
								SPA_PARAM_BUFFERS_align, SPA_POD_Int(16),
								SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(bufferTypes)));
	}
	else
	{
		updatedParams[1] = reinterpret_cast<spa_pod*> (spa_pod_builder_add_object(&updateBufferBuilder,
								SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
								SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(1, 1, 2),
								SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
								SPA_PARAM_BUFFERS_size, SPA_POD_Int(size),
								SPA_PARAM_BUFFERS_stride, SPA_POD_CHOICE_RANGE_Int(stride, stride, INT32_MAX),
								SPA_PARAM_BUFFERS_align, SPA_POD_Int(16),
								SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(bufferTypes)));
	}

	pw_thread_loop_lock(_pwMainThreadLoop);

	auto upParam = pw_stream_update_params(_pwStream, updatedParams, 2);
	qDebug().nospace() << "Pipewire: updated parameters " << upParam;
	_infoUpdate = DEFAULT_UPDATE_NUMBER;

	pw_thread_loop_unlock(_pwMainThreadLoop);
};

void PipewireHandler::onProcessFrame()
{
	if (auto cstate = pw_stream_get_state(_pwStream, nullptr); _incomingFrame != nullptr && cstate == PW_STREAM_STATE_STREAMING)
	{
		captureFrame();
	}

	if (auto cstate = pw_stream_get_state(_pwStream, nullptr); _incomingFrame != nullptr && (cstate == PW_STREAM_STATE_PAUSED || cstate == PW_STREAM_STATE_STREAMING))
	{
		pw_stream_queue_buffer(_pwStream, _incomingFrame);
		_incomingFrame = nullptr;
	}
};

void PipewireHandler::getImage(PipewireImage& retVal)
{
	if (hasError())
	{
		_image.isError = true;
		_image.data = nullptr;
	}

	retVal = _image;
}

void PipewireHandler::captureFrame()
{	
	struct pw_buffer* newFrame = _incomingFrame;

	_image.width = _frameWidth;
	_image.height = _frameHeight;
	_image.isOrderRgb = _frameOrderRgb;
	_image.version = getVersion();
	_image.isError = hasError();
	_image.stride = 0;

	if (_infoUpdate)
	{
		if (newFrame->buffer->datas->type == SPA_DATA_MemFd)
			qDebug().nospace() << "Pipewire: Using MemFD frame type. The hardware acceleration is DISABLED.";
		else if (newFrame->buffer->datas->type == SPA_DATA_DmaBuf)
			qDebug().nospace() << "Pipewire: Using DmaBuf frame type. The hardware acceleration is ENABLED.";
		else if (newFrame->buffer->datas->type == SPA_DATA_MemPtr)
			qDebug().nospace() << "Pipewire: Using MemPTR frame type. The hardware acceleration is DISABLED.";
	}

#ifdef ENABLE_PIPEWIRE_EGL
	if (newFrame->buffer->datas->type == SPA_DATA_DmaBuf)
	{
		if (!_initEGL)
		{
			qCritical().nospace() << "PipewireEGL: EGL is not initialized";
		}
		else if (newFrame->buffer->n_datas == 0 || newFrame->buffer->n_datas > 4)
		{
			qCritical().nospace() << "PipewireEGL: unexpected plane number";
		}
		else if (eglGetCurrentContext() != contextEgl && !eglMakeCurrent(displayEgl, EGL_NO_SURFACE, EGL_NO_SURFACE, contextEgl))
		{
			qCritical().nospace() << "PipewireEGL: failed to make a current context (reason = '" << eglErrorToString(eglGetError()) << "')";
		}
		else
		{
			QVector<EGLint> attribs;

			attribs << EGL_WIDTH << _frameWidth << EGL_HEIGHT << _frameHeight << EGL_LINUX_DRM_FOURCC_EXT << EGLint(_frameDrmFormat);

			EGLint EGL_DMA_BUF_PLANE_FD_EXT[] = { EGL_DMA_BUF_PLANE0_FD_EXT,     EGL_DMA_BUF_PLANE1_FD_EXT,     EGL_DMA_BUF_PLANE2_FD_EXT,     EGL_DMA_BUF_PLANE3_FD_EXT };
			EGLint EGL_DMA_BUF_PLANE_OFFSET_EXT[] = { EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT };
			EGLint EGL_DMA_BUF_PLANE_PITCH_EXT[] = { EGL_DMA_BUF_PLANE0_PITCH_EXT,  EGL_DMA_BUF_PLANE1_PITCH_EXT,  EGL_DMA_BUF_PLANE2_PITCH_EXT,  EGL_DMA_BUF_PLANE3_PITCH_EXT };
			EGLint EGL_DMA_BUF_PLANE_MODIFIER_LO_EXT[] = { EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT };
			EGLint EGL_DMA_BUF_PLANE_MODIFIER_HI_EXT[] = { EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT };

			for (uint32_t i = 0; i < newFrame->buffer->n_datas && i < 4; i++)
			{
				auto planes = newFrame->buffer->datas[i];

				attribs << EGL_DMA_BUF_PLANE_FD_EXT[i] << EGLint(planes.fd) << EGL_DMA_BUF_PLANE_OFFSET_EXT[i] << EGLint(planes.chunk->offset)
					<< EGL_DMA_BUF_PLANE_PITCH_EXT[i] << EGLint(planes.chunk->stride);

				if (_frameDrmModifier != DRM_FORMAT_MOD_INVALID)
				{
					attribs << EGL_DMA_BUF_PLANE_MODIFIER_LO_EXT[i] << EGLint(_frameDrmModifier & 0xffffffff)
						<< EGL_DMA_BUF_PLANE_MODIFIER_HI_EXT[i] << EGLint(_frameDrmModifier >> 32);
				}
			}

			attribs << EGL_IMAGE_PRESERVED_KHR << EGL_FALSE << EGL_NONE;

			auto eglImage = (newFrame->user_data != nullptr) ? static_cast<PipewireCache*>(newFrame->user_data)->eglImage :
				eglCreateImageKHR(displayEgl, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer) nullptr, attribs.data());

			if (eglImage == EGL_NO_IMAGE_KHR)
			{
				qCritical().nospace() << "PipewireEGL: failed to create a eglCreateImageKHR texture (reason = '" << eglErrorToString(eglGetError()) << "')";
			}
			else
			{
				if (newFrame->user_data == nullptr)
				{
					PipewireCache* cache = new PipewireCache();
					cache->eglImage = eglImage;
					newFrame->user_data = cache;
					qDebug().nospace() << "PipewireEGL: cached new eglCreateImageKHR texture";
				}

				GLenum glRes = GL_NO_ERROR;

				if (_eglTexture == 0)
				{
					glGenTextures(1, &_eglTexture);
					glRes = glGetError();

					if (glRes == GL_NO_ERROR)
					{
						if (_infoUpdate)
							qDebug().nospace() << "PipewireGL: target texture created";

						glBindTexture(GL_TEXTURE_2D, _eglTexture);
						glRes = glGetError();

						if (glRes == GL_NO_ERROR)
						{
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
						}
					}
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D, _eglTexture);
					glRes = glGetError();
				}

				if (glRes != GL_NO_ERROR)
				{
					qCritical().nospace() << "PipewireGL: could not create/bind target texture (" << glErrorToString(glRes) << ")";
				}
				else
				{
					glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
					glRes = glGetError();

					if (glRes != GL_NO_ERROR)
					{
						qCritical().nospace() << "PipewireGL: glEGLImageTargetTexture2DOES failed (" << glErrorToString(glRes) << ")";
					}
					else
					{
						if (_infoUpdate)
							qDebug().nospace() << "PipewireGL: glEGLImageTargetTexture2DOES OK";

						auto prefferedFormat = GL_RGBA;
						for (supportedDmaFormat& supVal : _supportedDmaFormatsList)
							if (_frameDrmFormat == supVal.drmFormat)
							{
								prefferedFormat = supVal.glFormat;
								break;
							}

						double ratio = static_cast<double>(std::max(_frameWidth, _frameHeight)) / std::max(_targetMaxSize, 1);
						_image.width = std::max(1, (int)(_frameWidth / ratio));
						_image.height = std::max(1, (int)(_frameHeight / ratio));

						if (_infoUpdate)
							qDebug().nospace() << "PipewireGL: scaling to " << _image.width << "x" << _image.height << " on GPU";;

						if (glRes == GL_NO_ERROR && (_eglFbos[0] == 0 || _eglFbos[1] == 0))
						{
							qDebug().nospace() << "PipewireGL: creating and caching eglFbos";
							glGenFramebuffers(static_cast<GLsizei>(_eglFbos.size()), _eglFbos.data());
							if (glRes = glGetError(); glRes != GL_NO_ERROR)
							{
								qCritical().nospace() << "PipewireGL: glGenFramebuffers failed (" << glErrorToString(glRes) << ")";
							}
						}

						if (glRes == GL_NO_ERROR && _eglScratchTex == 0)
						{
							qDebug().nospace() << "PipewireGL: creating and caching scratch texture";
							glGenTextures(1, &_eglScratchTex);
							if (glRes = glGetError(); glRes != GL_NO_ERROR)
							{
								qCritical().nospace() << "PipewireGL: glGenTextures for scratch texture failed (" << glErrorToString(glRes) << ")";
							}
							else
							{
								glBindTexture(GL_TEXTURE_2D, _eglScratchTex);
								glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _image.width, _image.height, 0, prefferedFormat, GL_UNSIGNED_BYTE, nullptr);
								if (glRes = glGetError(); glRes != GL_NO_ERROR)
								{
									qCritical().nospace() << "PipewireGL: glTexImage2D for scratch texture failed (" << glErrorToString(glRes) << ")";
								}
							}
						}

						if (glRes == GL_NO_ERROR && _eglFbos[0] != 0 && _eglFbos[1] != 0 && _eglScratchTex != 0)
						{
							glBindFramebuffer(GL_READ_FRAMEBUFFER, _eglFbos[0]);
							glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _eglTexture, 0);

							glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _eglFbos[1]);
							glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _eglScratchTex, 0);

							glBlitFramebuffer(0, 0, _frameWidth, _frameHeight, 0, 0, _image.width, _image.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
							if (glRes = glGetError(); glRes == GL_NO_ERROR)
							{
								_image.data = createMemory(_image.width * _image.height * 4);

								glBindFramebuffer(GL_FRAMEBUFFER, _eglFbos[1]);
								glPixelStorei(GL_PACK_ALIGNMENT, 1);
								glReadPixels(0, 0, _image.width, _image.height, prefferedFormat, GL_UNSIGNED_BYTE, _image.data);

								if (glRes = glGetError(); glRes != GL_NO_ERROR)
								{
									qCritical().nospace() << "PipewireGL: glReadPixels failed (" << glErrorToString(glRes) << ")";
								}

								glBindFramebuffer(GL_FRAMEBUFFER, 0);
							}
							else
							{
								qCritical().nospace() << "PipewireGL: glBlitFramebuffer failed (" << glErrorToString(glRes) << ")";
							}
						}
					}
				}
			}
		}
	}
	else
#endif
	{
		_image.stride = newFrame->buffer->datas->chunk->stride;

		if (newFrame->buffer->datas->type == SPA_DATA_MemFd)
		{
			auto mmapSize = (newFrame->user_data != nullptr) ? static_cast<PipewireCache*>(newFrame->user_data)->mmapSize :
				newFrame->buffer->datas->maxsize + newFrame->buffer->datas->mapoffset;
			auto mmapPtr = (newFrame->user_data != nullptr) ? static_cast<PipewireCache*>(newFrame->user_data)->mmapPtr :
				static_cast<uint8_t*>(mmap(nullptr, mmapSize, PROT_READ, MAP_PRIVATE, newFrame->buffer->datas->fd, 0));

			if (mmapPtr == MAP_FAILED)
			{
				qCritical().nospace() << "Pipewire: Failed to mmap the memory: " << strerror(errno);
			}
			else
			{
				if (_infoUpdate)
					qDebug().nospace() << "Pipewire: mapped memory";

				_image.data = createMemory(_image.stride * _frameHeight);
				memcpy(_image.data, mmapPtr, static_cast<size_t>(_image.stride)* _frameHeight);
				
				if (newFrame->user_data == nullptr)
				{
					PipewireCache* cache = new PipewireCache();
					cache->mmapSize = mmapSize;
					cache->mmapPtr = mmapPtr;
					newFrame->user_data = cache;
					qDebug().nospace() << "Pipewire: new mapped memory cached";
				}
			}
		}
		else if (newFrame->buffer->datas->type == SPA_DATA_MemPtr)
		{
			if (newFrame->buffer->datas[0].data == nullptr)
			{
				qCritical().nospace() << "Pipewire: empty buffer";
			}
			else
			{
				if (_infoUpdate)
					qDebug().nospace() << "Pipewire: prepare to copy already mapped memory";

				_image.data = createMemory(_image.stride * _frameHeight);
				memcpy(_image.data, static_cast<uint8_t*>(newFrame->buffer->datas[0].data), static_cast<size_t>(_image.stride) * _frameHeight);
			}
		}
	}

	// goodbye
	if (_infoUpdate > 0)
		_infoUpdate--;
};

uint8_t* PipewireHandler::createMemory(int size)
{
	_image.data = nullptr;
	_memoryCache.resize(size);

	return _memoryCache.data();
}

void PipewireHandler::onReleaseBuffer(struct pw_buffer* buffer)
{
	if (buffer->user_data != nullptr)
	{		
		PipewireCache* cache = static_cast<PipewireCache*>(buffer->user_data);

		qDebug().nospace() << "Pipewire: releasing cached texture/memory";

		if (cache->eglImage != EGL_NO_IMAGE_KHR && displayEgl != EGL_NO_DISPLAY)
		{
			if (eglGetCurrentContext() != contextEgl)
			{
				eglMakeCurrent(displayEgl, EGL_NO_SURFACE, EGL_NO_SURFACE, contextEgl);
				qDebug() << "Pipewire EGL: switch context";
			}
			eglDestroyImageKHR(displayEgl, cache->eglImage);
			qDebug() << "Pipewire EGL: delete image";
		}

		if (cache->mmapPtr != MAP_FAILED && cache->mmapSize > 0)
		{
			munmap(cache->mmapPtr, cache->mmapSize);
			qDebug() << "Pipewire: unmap memory";
		}

		delete cache;
		buffer->user_data = nullptr;
	}

	struct pw_buffer* expected = buffer;
	if (_incomingFrame.compare_exchange_strong(expected, nullptr))
	{
		qDebug() << "Pipewire: removing incoming frame because it's invalid now";
		if (auto cstate = pw_stream_get_state(_pwStream, nullptr); (cstate == PW_STREAM_STATE_PAUSED || cstate == PW_STREAM_STATE_STREAMING))
		{
			pw_stream_queue_buffer(_pwStream, buffer);
		}
	}
}

void PipewireHandler::onCoreError(uint32_t id, int seq, int res, const char *message)
{
	reportError(QString("Pipewire: core reports error '%1'").arg(message));
}

#ifdef ENABLE_PIPEWIRE_EGL

const char* PipewireHandler::glErrorToString(GLenum errorType)
{
	switch (errorType) {
	case GL_NO_ERROR:
		return "No error";
		break;
	case GL_INVALID_ENUM:
		return "An unacceptable value is specified for an enumerated argument (GL_INVALID_ENUM)";
		break;
	case GL_INVALID_VALUE:
		return "A numeric argument is out of range (GL_INVALID_VALUE)";
		break;
	case GL_INVALID_OPERATION:
		return "The specified operation is not allowed in the current state (GL_INVALID_OPERATION)";
		break;
	case GL_STACK_OVERFLOW:
		return "This command would cause a stack overflow (GL_STACK_OVERFLOW)";
		break;
	case GL_STACK_UNDERFLOW:
		return "This command would cause a stack underflow (GL_STACK_UNDERFLOW)";
		break;
	case GL_OUT_OF_MEMORY:
		return "There is not enough memory left to execute the command (GL_OUT_OF_MEMORY)";
		break;
	case GL_TABLE_TOO_LARGE:
		return "The specified table exceeds the implementation's maximum supported table size (GL_TABLE_TOO_LARGE)";
		break;
	default:
		return "An unknown error";
		break;
	}
}

const char* PipewireHandler::eglErrorToString(EGLint error_number)
{
	switch (error_number) {
	case EGL_SUCCESS:
		return "No error";
		break;
	case EGL_NOT_INITIALIZED:
		return "EGL is not initialized, or could not be initialized";
		break;
	case EGL_BAD_ACCESS:
		return "EGL cannot access a requested resource (thread context exception?)";
		break;
	case EGL_BAD_ALLOC:
		return "EGL failed to allocate resources";
		break;
	case EGL_BAD_ATTRIBUTE:
		return "An unrecognized attribute or value in the attribute list";
		break;
	case EGL_BAD_CONTEXT:
		return "An invalid EGLContext";
		break;
	case EGL_BAD_CONFIG:
		return "An invalid EGL frame buffer configuration for EGLConfig";
		break;
	case EGL_BAD_CURRENT_SURFACE:
		return "The current surface is no longer valid";
		break;
	case EGL_BAD_DISPLAY:
		return "An invalid EGL display connection";
		break;
	case EGL_BAD_SURFACE:
		return "The surface argument is nor properly configured for GL rendering";
		break;
	case EGL_BAD_MATCH:
		return "Arguments are inconsistent";
		break;
	case EGL_BAD_PARAMETER:
		return "Bad arguments";
		break;
	case EGL_BAD_NATIVE_PIXMAP:
		return "A NativePixmapType is invalid";
		break;
	case EGL_BAD_NATIVE_WINDOW:
		return "A NativeWindowType is invalid";
		break;
	case EGL_CONTEXT_LOST:
		return "The context is lost (a power management event has occurred?)";
		break;
	default:
		return "An unknown error";
		break;
	}
}

void PipewireHandler::initEGL()
{
	if (_initEGL)
		return;

	if (_libEglHandle == nullptr && (_libEglHandle = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL)) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: HyperHDR could not open EGL library";
		return;
	}

	if (_libGlHandle == nullptr && ((_libGlHandle = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL)) == nullptr)
							 && ((_libGlHandle = dlopen("libGL.so", RTLD_NOW | RTLD_GLOBAL)) == nullptr))
	{
		qCritical().nospace() << "PipewireEGL: HyperHDR could not open GL library";
		return;
	}

	if ((eglGetProcAddress = (eglGetProcAddressFun)dlsym(_libEglHandle, "eglGetProcAddress")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglGetProcAddress";
		return;
	}

	if ((eglGetPlatformDisplay = (eglGetPlatformDisplayFun)eglGetProcAddress("eglGetPlatformDisplay")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglGetPlatformDisplay";
		return;
	}

	if ((eglTerminate = (eglTerminateFun)eglGetProcAddress("eglTerminate")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglTerminate";
		return;
	}

	if ((eglInitialize = (eglInitializeFun)eglGetProcAddress("eglInitialize")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglInitialize";
		return;
	}

	if ((eglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXTFun)eglGetProcAddress("eglQueryDmaBufFormatsEXT")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglQueryDmaBufFormatsEXT";
		return;
	}

	if ((eglQueryDmaBufModifiersEXT = (eglQueryDmaBufModifiersEXTFun)eglGetProcAddress("eglQueryDmaBufModifiersEXT")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglQueryDmaBufModifiersEXT";
		return;
	}

	if ((eglCreateImageKHR = (eglCreateImageKHRFun)eglGetProcAddress("eglCreateImageKHR")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglCreateImageKHR";
		return;
	}

	if ((eglDestroyImageKHR = (eglDestroyImageKHRFun)eglGetProcAddress("eglDestroyImageKHR")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglDestroyImageKHR";
		return;
	}

	if ((eglCreateContext = (eglCreateContextFun)eglGetProcAddress("eglCreateContext")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglCreateContext";
		return;
	}

	if ((eglDestroyContext = (eglDestroyContextFun)eglGetProcAddress("eglDestroyContext")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglDestroyContext";
		return;
	}	

	if ((eglMakeCurrent = (eglMakeCurrentFun)eglGetProcAddress("eglMakeCurrent")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglMakeCurrent";
		return;
	}
	 
	if ((eglGetCurrentContext = (eglGetCurrentContextFun)eglGetProcAddress("eglGetCurrentContext")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglGetCurrentContext";
		return;
	}

	if ((eglGetError = (eglGetErrorFun)eglGetProcAddress("eglGetError")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglGetError";
		return;
	}

	if ((glEGLImageTargetTexture2DOES = (glEGLImageTargetTexture2DOESFun)eglGetProcAddress("glEGLImageTargetTexture2DOES")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get glEGLImageTargetTexture2DOES";
		return;
	}

	if ((eglBindAPI = (eglBindAPIFun)eglGetProcAddress("eglBindAPI")) == nullptr)
	{
		qCritical().nospace() << "PipewireEGL: failed to get eglBindAPI";
		return;
	}

	if ((glBindTexture = (glBindTextureFun)eglGetProcAddress("glBindTexture")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glBindTexture";
		return;
	}

	if ((glDeleteTextures = (glDeleteTexturesFun)eglGetProcAddress("glDeleteTextures")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glDeleteTextures";
		return;
	}

	if ((glGenTextures = (glGenTexturesFun)eglGetProcAddress("glGenTextures")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glGenTextures";
		return;
	}

	if ((glGetError = (glGetErrorFun)eglGetProcAddress("glGetError")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glGetError";
		return;
	}

	if ((glGetTexImage = (glGetTexImageFun)eglGetProcAddress("glGetTexImage")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glGetTexImage";
		return;
	}

	if ((glTexParameteri = (glTexParameteriFun)eglGetProcAddress("glTexParameteri")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glTexParameteri";
		return;
	}
	
	if ((glGenerateMipmap = (glGenerateMipmapFun)eglGetProcAddress("glGenerateMipmap")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glGenerateMipmap";
		return;
	}
	
	if ((glPixelStorei = (glPixelStoreiFun)eglGetProcAddress("glPixelStorei")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glPixelStorei";
		return;
	}
	if ((glGenFramebuffers = (glGenFramebuffersFun)eglGetProcAddress("glGenFramebuffers")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glGenFramebuffers";
		return;
	}
	if ((glBindFramebuffer = (glBindFramebufferFun)eglGetProcAddress("glBindFramebuffer")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glBindFramebuffer";
		return;
	}
	if ((glFramebufferTexture2D = (glFramebufferTexture2DFun)eglGetProcAddress("glFramebufferTexture2D")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glFramebufferTexture2D";
		return;
	}
	if ((glReadPixels = (glReadPixelsFun)eglGetProcAddress("glReadPixels")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glReadPixels";
		return;
	}
	if ((glDeleteFramebuffers = (glDeleteFramebuffersFun)eglGetProcAddress("glDeleteFramebuffers")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glDeleteFramebuffers";
		return;
	}
	if ((glBlitFramebuffer = (glBlitFramebufferFun)eglGetProcAddress("glBlitFramebuffer")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glBlitFramebuffer";
		return;
	}
	if ((glTexImage2D = (glTexImage2DFun)eglGetProcAddress("glTexImage2D")) == nullptr)
	{
		qCritical().nospace() << "PipewireGL: failed to get glTexImage2D";
		return;
	}


	if (displayEgl == EGL_NO_DISPLAY)
	{
		bool x11session = qgetenv("XDG_SESSION_TYPE") == QByteArrayLiteral("x11") && !qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
		qDebug().nospace() << "Session type: " << qgetenv("XDG_SESSION_TYPE") << " , X11 detected: " << ((x11session) ? "yes" : "no");

		displayEgl = (x11session) ? eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, (void*)EGL_DEFAULT_DISPLAY, nullptr) :
									eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, (void*)EGL_DEFAULT_DISPLAY, nullptr);

		if (displayEgl == EGL_NO_DISPLAY)
		{
			qCritical().nospace() << "PipewireEGL: no EGL display";
			return;
		}
		else
		{
			EGLint major, minor;
			if (eglInitialize(displayEgl, &major, &minor))
			{
				qDebug().nospace() << "PipewireEGL: EGL initialized for HyperHDR. Version: " << major << "." << minor;
			}
			else
			{
				qCritical().nospace() << "PipewireEGL: failed to init the display";
				return;
			}
		}
	}

	if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
	{
		qCritical().nospace() << "PipewireEGL: could not bing OpenGL API (reason = '" << eglErrorToString(eglGetError()) << "')";
		return;
	}

	if (contextEgl == EGL_NO_CONTEXT)
	{
		if ((contextEgl = eglCreateContext(displayEgl, nullptr, EGL_NO_CONTEXT, nullptr)) == EGL_NO_CONTEXT)
		{
			qCritical().nospace() << "PipewireEGL: Failed to create a context (reason = '" << eglErrorToString(eglGetError()) << "')";
			return;
		}
	}

	EGLint dmaCount = 0;

	if (!eglQueryDmaBufFormatsEXT(displayEgl, 0, nullptr, &dmaCount))
	{
		qCritical().nospace() << "PipewireEGL: Failed to query DMA-BUF format count (count = " << dmaCount << ", reason = '" << eglErrorToString(eglGetError()) << "')";
		return;
	}
	else if (dmaCount > 0)
	{
		qDebug().nospace() << "PipewireEGL: Found " << dmaCount  << " DMA-BUF formats";
	}

	
	QVector<uint32_t> dmaFormats(dmaCount);

	if (dmaCount > 0 && eglQueryDmaBufFormatsEXT(displayEgl, dmaCount, (EGLint*)dmaFormats.data(), &dmaCount))
	{
		qDebug().nospace() << "PipewireEGL: got DMA format list (count = " << dmaCount << ")";
	}
	else
	{
		qCritical().nospace() << "PipewireEGL: Failed to get DMA-BUF formats (reason = '" << eglErrorToString(eglGetError()) << "')";
		return;
	}

	QStringList supportedList;
	QStringList unsupportedList;

	foreach(const uint32_t& val, dmaFormats)
	{
		bool found = false;
		auto fourCC = fourCCtoString(val);

		for(supportedDmaFormat& supVal : _supportedDmaFormatsList)
			if (val == supVal.drmFormat)
			{
				EGLint modCount = 0;
				if (eglQueryDmaBufModifiersEXT(displayEgl, supVal.drmFormat, 0, nullptr, nullptr, &modCount) && modCount > 0)
				{
					supVal.modifiers = QVector<uint64_t>(modCount);
					if (eglQueryDmaBufModifiersEXT(displayEgl, supVal.drmFormat, modCount, supVal.modifiers.data(), nullptr, &modCount))
					{
						supportedList << QString("%1 (%2)").arg(supVal.friendlyName).arg(fourCC);
						supVal.hasDma = true;
						found = true;
						_initEGL = true;
						break;
					}
				}
			}

		if (!found)
		{
			unsupportedList << fourCC;
		}
	}


	if (!supportedList.isEmpty())
	{
		qDebug().nospace() << "PipewireEGL: Found supported by HyperHDR formats: " << supportedList.join(", ");
	}

	if (!unsupportedList.isEmpty())
	{
		qDebug().nospace() << "PipewireEGL: Found unsupported by HyperHDR formats: " << unsupportedList.join(", ");
	}
}
#endif

QString PipewireHandler::fourCCtoString(int64_t val)
{
	QString buf;
	if (val == DRM_FORMAT_MOD_INVALID)
	{
		return QString("None");
	}
	else for (int i = 0; i < 4; i++)
	{
		char c = (val >> (i * 8)) & 0xff;
		if (isascii(c) && isprint(c))
			buf.append(QChar(c));
		else
			buf.append(QChar('?'));
	}
	return buf;
}

pw_stream* PipewireHandler::getPipewireStream()
{
	return _pwStream;
}

bool PipewireHandler::hasIncomingFrame(struct pw_buffer* dequeueFrame)
{
	auto expected = static_cast<struct pw_buffer*>(nullptr);
	return !_incomingFrame.compare_exchange_strong(expected, dequeueFrame);
}

pw_stream* PipewireHandler::createCapturingStream()
{
	static const pw_stream_events pwStreamEvents = {
		.version = PW_VERSION_STREAM_EVENTS,
		.state_changed = [](void* handler, enum pw_stream_state old, enum pw_stream_state state, const char* error) {
			emit reinterpret_cast<PipewireHandler*>(handler)->onStateChangedSignal(old, state, error);
		},		
		.param_changed = [](void* handler, uint32_t id, const struct spa_pod* param) {
			emit reinterpret_cast<PipewireHandler*>(handler)->onParamsChangedSignal(id, param);
		},
		.remove_buffer = [](void* handler, struct pw_buffer* b) {
			auto master = reinterpret_cast<PipewireHandler*>(handler);
			BLOCK_CALL_1(master, onReleaseBuffer, struct pw_buffer*, b);
		},
		.process = [](void *handler) {
			auto master = reinterpret_cast<PipewireHandler*>(handler);
			if (auto pwStream = master->getPipewireStream(); pwStream != nullptr)
			{
				if (auto dequeueFrame = pw_stream_dequeue_buffer(pwStream); dequeueFrame != nullptr)
				{
					
					if (!master->hasIncomingFrame(dequeueFrame))
					{
						emit master->onProcessFrameSignal();
					}
					else
					{
						qDebug().nospace() << "Pipewire: skipping new frame processing - the queue is full";
						pw_stream_queue_buffer(pwStream, dequeueFrame);
					}
				}
			}
		}
	};

	static const pw_core_events pwCoreEvents = {
		.version = PW_VERSION_CORE_EVENTS,
		.info = [](void *user_data, const struct pw_core_info *info) {
			qDebug().nospace() << "Pipewire: core info reported. Version = " << info->version;
		},		
		.error = [](void *handler, uint32_t id, int seq, int res, const char *message) {
			qCritical().nospace() << "Pipewire: core error reported";
			emit reinterpret_cast<PipewireHandler*>(handler)->onCoreErrorSignal(id, seq, res, message);
		},
	};

	spa_rectangle pwScreenBoundsMin = SPA_RECTANGLE(1, 1);
	spa_rectangle pwScreenBoundsDefault = SPA_RECTANGLE(320, 200);
	spa_rectangle pwScreenBoundsMax = SPA_RECTANGLE(8192, 8192);

	spa_fraction pwFramerateMin = SPA_FRACTION(0, 1);
	spa_fraction pwFramerateDefault = SPA_FRACTION(_requestedFPS, 1);
	spa_fraction pwFramerateMax = SPA_FRACTION(60, 1);

	pw_properties* reuseProps = pw_properties_new_string("pipewire.client.reuse=1");
	
	pw_core_add_listener(_pwContextConnection, &_pwCoreListener, &pwCoreEvents, this);

	pw_stream* stream = pw_stream_new(_pwContextConnection, "hyperhdr-stream-receiver", reuseProps);

	if (stream != nullptr)
	{
		std::vector<spa_pod*> streamParams;
		
		MemoryBuffer<uint8_t> spaBufferMem(3072);

		auto spaBuilder = SPA_POD_BUILDER_INIT(spaBufferMem.data(), static_cast<uint32_t>(spaBufferMem.size()));

		#ifdef ENABLE_PIPEWIRE_EGL

		if (_enableEGL)
		{
			initEGL();

			for (const supportedDmaFormat& val : _supportedDmaFormatsList)
				if (val.hasDma)
				{
					spa_pod_frame frameDMA[2];
					spa_pod_builder_push_object(&spaBuilder, &frameDMA[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
					spa_pod_builder_add(&spaBuilder, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
					spa_pod_builder_add(&spaBuilder, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
					spa_pod_builder_add(&spaBuilder, SPA_FORMAT_VIDEO_format, SPA_POD_Id(val.spaFormat), 0);

					if (val.modifiers.count() == 1 && val.modifiers[0] == DRM_FORMAT_MOD_INVALID)
					{
						spa_pod_builder_prop(&spaBuilder, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY);
						spa_pod_builder_long(&spaBuilder, val.modifiers[0]);
					}
					else if (val.modifiers.count() > 0)
					{
						spa_pod_builder_prop(&spaBuilder, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
						spa_pod_builder_push_choice(&spaBuilder, &frameDMA[1], SPA_CHOICE_Enum, 0);
						spa_pod_builder_long(&spaBuilder, val.modifiers[0]);
						for (auto modVal : val.modifiers)
						{
							spa_pod_builder_long(&spaBuilder, modVal);
						}
						spa_pod_builder_pop(&spaBuilder, &frameDMA[1]);
					}

					spa_pod_builder_add(&spaBuilder, SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&pwScreenBoundsDefault, &pwScreenBoundsMin, &pwScreenBoundsMax), 0);
					spa_pod_builder_add(&spaBuilder, SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&pwFramerateDefault, &pwFramerateMin, &pwFramerateMax), 0);
					streamParams.push_back(reinterpret_cast<spa_pod*> (spa_pod_builder_pop(&spaBuilder, &frameDMA[0])));
				}
		}
		#endif

		streamParams.push_back( reinterpret_cast<spa_pod*> (spa_pod_builder_add_object(&spaBuilder,
			SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
			SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
			SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
			SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(5,
				SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA,
				SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
			SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&pwScreenBoundsDefault, &pwScreenBoundsMin, &pwScreenBoundsMax),
			SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&pwFramerateDefault, &pwFramerateMin, &pwFramerateMax))) );

		pw_stream_add_listener(stream, &_pwStreamListener, &pwStreamEvents, this);

		int res = pw_stream_connect(stream, PW_DIRECTION_INPUT, _streamNodeId, static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT), const_cast<const spa_pod**>(streamParams.data()), streamParams.size());

		if ( res != 0)
		{
			qCritical().nospace() << "Pipewire: could not connect to the stream. Error code: " << res;
			pw_stream_destroy(stream);
			stream = nullptr;
		}
		else
			qDebug().nospace() << "Pipewire: the stream is connected";
	}

	return stream;
}

