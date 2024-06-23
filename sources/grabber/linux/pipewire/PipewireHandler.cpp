/* PipewireHandler.cpp
* 
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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


#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <cassert>
#include <sys/wait.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <cstring>
#include <iostream>
#include <dlfcn.h>

#include <grabber/linux/pipewire/smartPipewire.h>
#include <grabber/linux/pipewire/PipewireHandler.h>
#include <grabber/linux/pipewire/ScreenCastProxy.h>
#include <utils/Macros.h>

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

const QString DESKTOP_SERVICE	 = QStringLiteral("org.freedesktop.portal.Desktop");
const QString DESKTOP_PATH		 = QStringLiteral("/org/freedesktop/portal/desktop");
const QString DESKTOP_SCREENCAST = QStringLiteral("org.freedesktop.portal.ScreenCast");
const QString PORTAL_REQUEST	 = QStringLiteral("org.freedesktop.portal.Request");
const QString PORTAL_SESSION	 = QStringLiteral("org.freedesktop.portal.Session");
const QString PORTAL_RESPONSE	 = QStringLiteral("Response");


const QString REQUEST_TEMPLATE = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2");

PipewireHandler::PipewireHandler() :
									_sessionHandle(""), _restorationToken(""), _errorMessage(""), _portalStatus(false),
									_isError(false), _version(-1), _streamNodeId(0),
									_sender(""), _replySessionPath(""), _sourceReplyPath(""), _startReplyPath(""),
									_pwMainThreadLoop(nullptr), _pwNewContext(nullptr), _pwContextConnection(nullptr), _pwStream(nullptr),
									_frameWidth(0),_frameHeight(0),_frameOrderRgb(false), _framePaused(false), _requestedFPS(10), _hasFrame(false),
									_infoUpdated(false), _initEGL(false), _libEglHandle(NULL), _libGlHandle(NULL),
									_frameDrmFormat(DRM_FORMAT_MOD_INVALID), _frameDrmModifier(DRM_FORMAT_MOD_INVALID), _image()
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

	if (_libEglHandle != NULL)
	{
		dlclose(_libEglHandle);
		_libEglHandle = NULL;
	}

	if (_libGlHandle != NULL)
	{
		dlclose(_libGlHandle);
		_libGlHandle = NULL;
	}
}

void PipewireHandler::closeSession()
{
	if (_pwMainThreadLoop != nullptr)
	{
		pw_thread_loop_wait(_pwMainThreadLoop);
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
			sdbus::ServiceName destination{DESKTOP_SERVICE.toStdString()};
			sdbus::ObjectPath objectPath{_sessionHandle.toStdString()};
			auto sessionProxy = sdbus::createProxy(*_dbusConnection, std::move(destination), std::move(objectPath));
			auto call = sessionProxy->createMethodCall(sdbus::InterfaceName{PORTAL_SESSION.toStdString()}, sdbus::MethodName {"Close"});
			auto reply = sessionProxy->callMethod(call);
		}
		catch (std::exception& e)
		{
			std::cout << "Pipewire: could not close session: " << e.what() << std::endl;
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
	_framePaused = false;
	_requestedFPS = 10;
	_hasFrame = false;
	_infoUpdated = false;
	_frameDrmFormat = DRM_FORMAT_MOD_INVALID;
	_frameDrmModifier = DRM_FORMAT_MOD_INVALID;

#ifdef ENABLE_PIPEWIRE_EGL
	if (contextEgl != EGL_NO_CONTEXT)
	{
		auto rel = eglMakeCurrent(displayEgl, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		std::cout << "PipewireEGL: releasing EGL context => " << rel << std::endl;

		auto res = eglDestroyContext(displayEgl, contextEgl);
		std::cout << "PipewireEGL: destroying EGL context => " << res << std::endl;
		contextEgl = EGL_NO_CONTEXT;
	}

	if (displayEgl != EGL_NO_DISPLAY)
	{
		auto res = eglTerminate(displayEgl);
		std::cout << "PipewireEGL: terminate the display => " << res << std::endl;		
		displayEgl = EGL_NO_DISPLAY;
	}

	_initEGL = false;
#endif

	for (supportedDmaFormat& supVal : _supportedDmaFormatsList)
		supVal.hasDma = false;

	releaseWorkingFrame();

	createMemory(0);

	std::cout << "Pipewire: driver is closed now" << std::endl;
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
	std::cerr << qPrintable(input) << std::endl;
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
		auto proxy = std::make_unique<ScreenCastProxy>(*bus, ServiceName{ DESKTOP_SERVICE.toStdString() }, ObjectPath{ DESKTOP_PATH.toStdString() });

		version = proxy->version();
	}
	catch (std::exception& e)
	{
		std::cout << "Pipewire: could not read Portal ScreenCast version" << std::endl;
		version = -1;
	}

	return version;
}

void PipewireHandler::startSession(QString restorationToken, uint32_t requestedFPS)
{
	std::cout << "Pipewire: initialization invoked. Cleaning up first..." << std::endl;

	closeSession();

	if (requestedFPS < 1 || requestedFPS > 60)
	{
		reportError("Pipewire: invalid capture rate.");
		return;
	}

	try
	{
		_dbusConnection = sdbus::createSessionBusConnection();
		_screenCastProxy = std::make_unique<ScreenCastProxy>(*_dbusConnection, ServiceName{ DESKTOP_SERVICE.toStdString() }, ObjectPath{ DESKTOP_PATH.toStdString() });
		_dbusConnection->enterEventLoopAsync();

		_version = _screenCastProxy->version();
	}
	catch(std::exception& e)
	{
		std::cout << "Pipewire: could not read Portal ScreenCast version" << std::endl;
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

	std::cout << "Sender: " << qPrintable(_sender) << std::endl;

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
				std::cout << "Create session didnt return a handle" << std::endl;
			}
			else
			{
				QString session = QString::fromStdString(sessionHandleIter->second.get<std::string>());
				QUEUE_CALL_2(this, createSessionResponse, uint, resultCode, QString, session );
			}
		};

		_createSessionProxy = sdbus::createProxy(*_dbusConnection, sdbus::ServiceName{""}, sdbus::ObjectPath{_replySessionPath.toStdString()});

		_createSessionProxy->uponSignal(SignalName{PORTAL_RESPONSE.toStdString()}).onInterface(InterfaceName{PORTAL_REQUEST.toStdString()}).call(responseSignalHandler);

        _screenCastProxy->CreateSession(createSessionParams);

    }
	catch(std::exception& ex)
    {
        reportError(QString("Pipewire: Failed to create session: %1").arg(QString::fromLocal8Bit(ex.what())));
    }
	std::cout << "Requested FPS: " << _requestedFPS << std::endl;
	std::cout << "Pipewire: CreateSession finished" << std::endl;
}


void PipewireHandler::createSessionResponse(uint response, QString session)
{
	std::cout << "Pipewire: Got response from portal CreateSession" << std::endl;

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
		std::cout << "Pipewire: Has restoration token: " << qPrintable(QString(_restorationToken).right(12)) << std::endl;
	}

	try
	{
		auto responseSignalHandler = [&] (uint32_t resultCode, std::map<std::string, sdbus::Variant> results)
		{
			QUEUE_CALL_1(this, selectSourcesResponse, uint, resultCode);
		};

		_selectSourceProxy = sdbus::createProxy(*_dbusConnection, sdbus::ServiceName{""}, sdbus::ObjectPath{_sourceReplyPath.toStdString()});
		_selectSourceProxy->uponSignal(SignalName{PORTAL_RESPONSE.toStdString()}).onInterface(InterfaceName{PORTAL_REQUEST.toStdString()}).call(responseSignalHandler);

		_screenCastProxy->SelectSources(ObjectPath{_sessionHandle.toStdString()}, selectSourceParams);

	}
	catch(std::exception& ex)
	{
		reportError(QString("Pipewire: Failed to select a source: %1").arg(QString::fromLocal8Bit(ex.what())));
	}

	std::cout << "Pipewire: SelectSources finished" << std::endl;
}


void PipewireHandler::selectSourcesResponse(uint response)
{
	std::cout << "Pipewire: Got response from portal SelectSources" << std::endl;

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
					std::cout << "Start session didnt return a restoration handle" << std::endl;
				}
				else
				{
					restoreHandle = QString::fromStdString(restoreHandleIter->second.get<std::string>());
				}

				auto streamsIter = results.find("streams");
				if (streamsIter == results.end())
				{
					std::cout << "Start session didnt return streams" << std::endl;
				}
				else
				{
					std::vector<sdbus::Struct<uint32_t, std::map<std::string, sdbus::Variant>>> streams =
						streamsIter->second.get<std::vector<sdbus::Struct<uint32_t, std::map<std::string, sdbus::Variant>>>>();
					if (streams.empty())
					{
						std::cout << "Start session didnt return any stream" << std::endl;
					}

					auto stream = streams[0];
					int nodeStreamWidth = 0;
					int nodeStreamHeight = 0;
					uint32_t nodeId = stream.get<0>();
					auto nodeStruct = stream.get<1>();

					auto sizeIter = nodeStruct.find("size");
					if (sizeIter == nodeStruct.end())
					{
						std::cout << "Could not read stream size" << std::endl;
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
		_startProxy->uponSignal(SignalName{PORTAL_RESPONSE.toStdString()}).onInterface(InterfaceName{PORTAL_REQUEST.toStdString()}).call(responseSignalHandler);

		_screenCastProxy->Start( ObjectPath{_sessionHandle.toStdString()}, "", startParams);

	}
	catch(std::exception& ex)
	{
		reportError(QString("Pipewire: Failed to select a source: %1").arg(QString::fromLocal8Bit(ex.what())));
	}

	
	std::cout << "Pipewire: Start finished" << std::endl;
}

void PipewireHandler::startResponse(uint response, QString restoreHandle, uint32_t nodeId, int nodeStreamWidth, int nodeStreamHeight)
{
	std::cout << "Pipewire: Got response from portal Start" << std::endl;

	if (response != 0)
	{		
		reportError(QString("Pipewire: Failed to start or cancel dialog: %1").arg(response));
		_sessionHandle = "";
		return;
	}

	if (!restoreHandle.isEmpty())
	{
		_restorationToken = restoreHandle;
		std::cout << "Received restoration token: " << qPrintable(QString(_restorationToken).right(12)) << std::endl;
	}
	else
		std::cout << "No restoration token (portal protocol version 4 required and must be implemented by the backend GNOME/KDE... etc)" << std::endl;


	_streamNodeId = nodeId;
	_frameWidth = nodeStreamWidth;
	_frameHeight = nodeStreamHeight;
	_portalStatus = true;
	
	//------------------------------------------------------------------------------------
	std::cout << "Connecting to Pipewire interface for stream: " << _frameWidth << " x " << _frameHeight << std::endl;

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
	if (state != PW_STREAM_STATE_STREAMING && _pwStream != nullptr)
	{		
		_framePaused = true;
	}

	if (state == PW_STREAM_STATE_STREAMING)
	{
		_framePaused = false;
	}

	if (state == PW_STREAM_STATE_ERROR)
	{
		reportError(QString("Pipewire: stream reports error '%1'").arg(error));
		return;
	}

	if (state == PW_STREAM_STATE_UNCONNECTED) printf("Pipewire: state UNCONNECTED (%d, %d)\n", (int)state, (int)old);
	else if (state == PW_STREAM_STATE_CONNECTING) printf("Pipewire: state CONNECTING (%d, %d)\n", (int)state, (int)old);
	else if (state == PW_STREAM_STATE_PAUSED) printf("Pipewire: state PAUSED (%d, %d)\n", (int)state, (int)old);
	else if (state == PW_STREAM_STATE_STREAMING) printf("Pipewire: state STREAMING (%d, %d)\n", (int)state, (int)old);
	else printf("Pipewire: state UNKNOWN (%d, %d)\n", (int)state, (int)old);
}

void PipewireHandler::onParamsChanged(uint32_t id, const struct spa_pod* param)
{
	struct spa_video_info format {};

	std::cout << "Pipewire: got new video format selected" << std::endl;

	if (param == nullptr || id != SPA_PARAM_Format)
		return;

	if (spa_format_parse(param, &format.media_type, &format.media_subtype) < 0)
		return;

	if (format.media_type != SPA_MEDIA_TYPE_video || format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
		return;

	if (spa_format_video_raw_parse(param, &format.info.raw) < 0)
		return;

	_frameWidth = format.info.raw.size.width;
	_frameHeight = format.info.raw.size.height;
	_frameDrmModifier = format.info.raw.modifier;
	_frameOrderRgb = (format.info.raw.format == SPA_VIDEO_FORMAT_RGBx || format.info.raw.format == SPA_VIDEO_FORMAT_RGBA);

	for (const supportedDmaFormat& val : _supportedDmaFormatsList)
		if (val.spaFormat == format.info.raw.format)
		{
			_frameDrmFormat = val.drmFormat;
			break;
		}

	printf("Pipewire: video format = %d (%s)\n", format.info.raw.format, spa_debug_type_find_name(spa_type_video_format, format.info.raw.format));
	printf("Pipewire: video size = %dx%d (RGB order = %s)\n", _frameWidth, _frameHeight, (_frameOrderRgb) ? "true" : "false");
	printf("Pipewire: framerate = %d/%d\n", format.info.raw.framerate.num, format.info.raw.framerate.denom);

	uint8_t updateBuffer[1000];
	struct spa_pod_builder updateBufferBuilder = SPA_POD_BUILDER_INIT(updateBuffer, sizeof(updateBuffer));
	const struct spa_pod* updatedParams[1];
	const uint32_t stride = SPA_ROUND_UP_N(_frameWidth * 4, 4);
	const int32_t size = _frameHeight * stride;

	const auto bufferTypes = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier)
		? (1 << SPA_DATA_DmaBuf) | (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr)
		: (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr);

	const bool hasDma = (bufferTypes & (1 << SPA_DATA_DmaBuf));

	// display capabilities
	if (hasDma)
	{
		printf("Pipewire: DMA buffer available. Format: %s. Modifier: %s.\n",
				fourCCtoString(_frameDrmFormat).toLocal8Bit().constData(),
				fourCCtoString(_frameDrmModifier).toLocal8Bit().constData());
	}
	if (bufferTypes & (1 << SPA_DATA_MemFd))
	{
		printf("Pipewire: MemFD buffer available\n");
	}
	if (bufferTypes & (1 << SPA_DATA_MemPtr))
	{
		printf("Pipewire: MemPTR buffer available\n");
	}

	if (hasDma)
	{
		updatedParams[0] = reinterpret_cast<spa_pod*> (spa_pod_builder_add_object(&updateBufferBuilder,
								SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
								SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(2, 2, 16),
								SPA_PARAM_BUFFERS_size, SPA_POD_Int(size),
								SPA_PARAM_BUFFERS_stride, SPA_POD_CHOICE_RANGE_Int(stride, stride, INT32_MAX),
								SPA_PARAM_BUFFERS_align, SPA_POD_Int(16),
								SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(bufferTypes)));
	}
	else
	{
		updatedParams[0] = reinterpret_cast<spa_pod*> (spa_pod_builder_add_object(&updateBufferBuilder,
								SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
								SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(2, 2, 16),
								SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
								SPA_PARAM_BUFFERS_size, SPA_POD_Int(size),
								SPA_PARAM_BUFFERS_stride, SPA_POD_CHOICE_RANGE_Int(stride, stride, INT32_MAX),
								SPA_PARAM_BUFFERS_align, SPA_POD_Int(16),
								SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(bufferTypes)));
	}

	pw_thread_loop_lock(_pwMainThreadLoop);
	
	printf("Pipewire: updated parameters %d\n", pw_stream_update_params(_pwStream, updatedParams, 1));	
	_infoUpdated = false;

	pw_thread_loop_unlock(_pwMainThreadLoop);
};

void PipewireHandler::onProcessFrame()
{
	captureFrame();
	_hasFrame = (_image.data != nullptr);
};

void PipewireHandler::getImage(PipewireImage& retVal)
{
	if (_framePaused)
	{
		_image.isError = hasError();
		_image.data = nullptr;
	}

	retVal = _image;
}

void PipewireHandler::captureFrame()
{	
	struct pw_buffer* newFrame = nullptr;
	struct pw_buffer* dequeueFrame = nullptr;
	uint8_t* mappedMemory = nullptr;
	uint8_t* frameBuffer = nullptr;	

	if (_pwStream == nullptr || _framePaused)
		return;

	_hasFrame = false;

	_image.width = _frameWidth;
	_image.height = _frameHeight;
	_image.isOrderRgb = _frameOrderRgb;
	_image.version = getVersion();
	_image.isError = hasError();
	_image.stride = 0;

	while ((dequeueFrame = pw_stream_dequeue_buffer(_pwStream)) != nullptr)
	{
		if (newFrame != nullptr && pw_stream_get_state(_pwStream, NULL) == PW_STREAM_STATE_STREAMING)
			pw_stream_queue_buffer(_pwStream, newFrame);
		newFrame = dequeueFrame;
	}

	if (newFrame != nullptr)
	{
		if (!_infoUpdated)
		{
			if (newFrame->buffer->datas->type == SPA_DATA_MemFd)
				printf("Pipewire: Using MemFD frame type. The hardware acceleration is DISABLED.\n");
			else if (newFrame->buffer->datas->type == SPA_DATA_DmaBuf)
				printf("Pipewire: Using DmaBuf frame type. The hardware acceleration is ENABLED.\n");
			else if (newFrame->buffer->datas->type == SPA_DATA_MemPtr)
				printf("Pipewire: Using MemPTR frame type. The hardware acceleration is DISABLED.\n");			
		}

#ifdef ENABLE_PIPEWIRE_EGL
		if (newFrame->buffer->datas->type == SPA_DATA_DmaBuf)
		{
			if (!_initEGL)
			{
				printf("PipewireEGL: EGL is not initialized\n");
			}
			else if (newFrame->buffer->n_datas == 0 || newFrame->buffer->n_datas > 4)
			{
				printf("PipewireEGL: unexpected plane number\n");
			}
			else if (!eglMakeCurrent(displayEgl, EGL_NO_SURFACE, EGL_NO_SURFACE, contextEgl))
			{
				printf("PipewireEGL: failed to make a current context (reason = '%s')\n", eglErrorToString(eglGetError()));
			}
			else
			{				
				QVector<EGLint> attribs;
				
				attribs << EGL_WIDTH << _frameWidth << EGL_HEIGHT << _frameHeight << EGL_LINUX_DRM_FOURCC_EXT << EGLint(_frameDrmFormat);

				EGLint EGL_DMA_BUF_PLANE_FD_EXT[]     = { EGL_DMA_BUF_PLANE0_FD_EXT,     EGL_DMA_BUF_PLANE1_FD_EXT,     EGL_DMA_BUF_PLANE2_FD_EXT,     EGL_DMA_BUF_PLANE3_FD_EXT };
				EGLint EGL_DMA_BUF_PLANE_OFFSET_EXT[] = { EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT };
				EGLint EGL_DMA_BUF_PLANE_PITCH_EXT[]  = { EGL_DMA_BUF_PLANE0_PITCH_EXT,  EGL_DMA_BUF_PLANE1_PITCH_EXT,  EGL_DMA_BUF_PLANE2_PITCH_EXT,  EGL_DMA_BUF_PLANE3_PITCH_EXT };
				EGLint EGL_DMA_BUF_PLANE_MODIFIER_LO_EXT[] = { EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT };
				EGLint EGL_DMA_BUF_PLANE_MODIFIER_HI_EXT[] = { EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT };

				for(uint32_t i = 0; i < newFrame->buffer->n_datas && i < 4; i++)
				{
					auto planes = newFrame->buffer->datas[i];

					attribs << EGL_DMA_BUF_PLANE_FD_EXT[i] << EGLint(planes.fd) << EGL_DMA_BUF_PLANE_OFFSET_EXT[i] << EGLint(planes.chunk->offset)
							<< EGL_DMA_BUF_PLANE_PITCH_EXT[i] << EGLint(planes.chunk->stride);

					if (_frameDrmModifier != DRM_FORMAT_MOD_INVALID)
					{
						attribs	<< EGL_DMA_BUF_PLANE_MODIFIER_LO_EXT[i] << EGLint(_frameDrmModifier & 0xffffffff)
								<< EGL_DMA_BUF_PLANE_MODIFIER_HI_EXT[i] << EGLint(_frameDrmModifier >> 32);
					}
				}

				attribs << EGL_IMAGE_PRESERVED_KHR << EGL_TRUE << EGL_NONE;

				EGLImage eglImage = eglCreateImageKHR(displayEgl, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer) nullptr, attribs.data());

				if (eglImage == EGL_NO_IMAGE_KHR)
				{
					printf("PipewireEGL: failed to create a texture (reason = '%s')\n", eglErrorToString(eglGetError()));
				}
				else
				{
					if (!_infoUpdated)
						printf("PipewireEGL: got the texture\n");

					GLenum glRes;
					GLuint texture;

					glGenTextures(1, &texture);
					glRes = glGetError();

					if (glRes != GL_NO_ERROR)
					{
						printf("PipewireGL: could not render create a texture (%s)", glErrorToString(glRes));
					}
					else
					{
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
						glBindTexture(GL_TEXTURE_2D, texture);

						glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
						glRes = glGetError();

						if (glRes != GL_NO_ERROR)
						{
							printf("PipewireGL: glEGLImageTargetTexture2DOES failed (%s)\n", glErrorToString(glRes));
						}
						else
						{
							frameBuffer = createMemory(newFrame->buffer->datas[0].chunk->stride * _frameHeight);

							for (supportedDmaFormat& supVal : _supportedDmaFormatsList)
								if (_frameDrmFormat == supVal.drmFormat)
								{
									glGetTexImage(GL_TEXTURE_2D, 0, supVal.glFormat, GL_UNSIGNED_BYTE, frameBuffer);
									glRes = glGetError();
									
									if (glRes != GL_NO_ERROR)
									{
										printf("PipewireGL: could not render the DMA texture (%s)\n", glErrorToString(glRes));
									}
									else
									{
										if (!_infoUpdated)
											printf("PipewireEGL: succesfully rendered the DMA texture\n");

										_image.data = frameBuffer;
									}

									break;
								}
						}

						glDeleteTextures(1, &texture);					
					}
					eglDestroyImageKHR(displayEgl, eglImage);
				}
			}
		}
		else
#endif
		if (newFrame->buffer->datas[0].data == nullptr)
		{
			printf("Pipewire: empty buffer\n");
		}
		else
		{
			_image.stride = newFrame->buffer->datas->chunk->stride;

			if (newFrame->buffer->datas->type == SPA_DATA_MemFd)
			{
				mappedMemory = static_cast<uint8_t*>(mmap(nullptr,
												newFrame->buffer->datas->maxsize + newFrame->buffer->datas->mapoffset,
												PROT_READ, MAP_PRIVATE, newFrame->buffer->datas->fd, 0));

				if (mappedMemory == MAP_FAILED)
				{
					printf("Pipewire: Failed to mmap the memory: %s ", strerror(errno));
					mappedMemory = nullptr;
				}
				else
				{
					_image.data = createMemory(_image.stride * _frameHeight);
					memcpy(_image.data, mappedMemory, static_cast<size_t>(_image.stride) * _frameHeight);
				}
			}
			else if (newFrame->buffer->datas->type == SPA_DATA_MemPtr)
			{				
				_image.data = createMemory(_image.stride * _frameHeight);
				memcpy(_image.data, static_cast<uint8_t*>(newFrame->buffer->datas[0].data), static_cast<size_t>(_image.stride) * _frameHeight);
			}
		}
	}

	// clean up
	if (mappedMemory != nullptr)
		munmap(mappedMemory, newFrame->buffer->datas->maxsize + newFrame->buffer->datas->mapoffset);

	if (newFrame != nullptr && pw_stream_get_state(_pwStream, NULL) == PW_STREAM_STATE_STREAMING)
		pw_stream_queue_buffer(_pwStream, newFrame);

	// goodbye
	_infoUpdated = true;
};

uint8_t* PipewireHandler::createMemory(int size)
{
	_image.data = nullptr;
	_memoryCache.resize(size);

	return _memoryCache.data();
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

	if (_libEglHandle == NULL && (_libEglHandle = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL)) == NULL)
	{
		printf("PipewireEGL: HyperHDR could not open EGL library\n");
		return;
	}

	if (_libGlHandle == NULL && ((_libGlHandle = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL)) == NULL)
							 && ((_libGlHandle = dlopen("libGL.so", RTLD_NOW | RTLD_GLOBAL)) == NULL))
	{
		printf("PipewireEGL: HyperHDR could not open GL library\n");
		return;
	}

	if ((eglGetProcAddress = (eglGetProcAddressFun)dlsym(_libEglHandle, "eglGetProcAddress")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglGetProcAddress\n");
		return;
	}

	if ((eglGetPlatformDisplay = (eglGetPlatformDisplayFun)eglGetProcAddress("eglGetPlatformDisplay")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglGetPlatformDisplay\n");
		return;
	}

	if ((eglTerminate = (eglTerminateFun)eglGetProcAddress("eglTerminate")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglTerminate\n");
		return;
	}

	if ((eglInitialize = (eglInitializeFun)eglGetProcAddress("eglInitialize")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglInitialize\n");
		return;
	}

	if ((eglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXTFun)eglGetProcAddress("eglQueryDmaBufFormatsEXT")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglQueryDmaBufFormatsEXT\n");
		return;
	}

	if ((eglQueryDmaBufModifiersEXT = (eglQueryDmaBufModifiersEXTFun)eglGetProcAddress("eglQueryDmaBufModifiersEXT")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglQueryDmaBufModifiersEXT\n");
		return;
	}

	if ((eglCreateImageKHR = (eglCreateImageKHRFun)eglGetProcAddress("eglCreateImageKHR")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglCreateImageKHR\n");
		return;
	}

	if ((eglDestroyImageKHR = (eglDestroyImageKHRFun)eglGetProcAddress("eglDestroyImageKHR")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglDestroyImageKHR\n");
		return;
	}

	if ((eglCreateContext = (eglCreateContextFun)eglGetProcAddress("eglCreateContext")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglCreateContext\n");
		return;
	}

	if ((eglDestroyContext = (eglDestroyContextFun)eglGetProcAddress("eglDestroyContext")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglDestroyContext\n");
		return;
	}	

	if ((eglMakeCurrent = (eglMakeCurrentFun)eglGetProcAddress("eglMakeCurrent")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglMakeCurrent\n");
		return;
	}

	if ((eglGetError = (eglGetErrorFun)eglGetProcAddress("eglGetError")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglGetError\n");
		return;
	}

	if ((glEGLImageTargetTexture2DOES = (glEGLImageTargetTexture2DOESFun)eglGetProcAddress("glEGLImageTargetTexture2DOES")) == nullptr)
	{
		printf("PipewireEGL: failed to get glEGLImageTargetTexture2DOES\n");
		return;
	}

	if ((eglBindAPI = (eglBindAPIFun)eglGetProcAddress("eglBindAPI")) == nullptr)
	{
		printf("PipewireEGL: failed to get eglBindAPI\n");
		return;
	}	

	if ((glXGetProcAddressARB = (glXGetProcAddressARBFun)dlsym(_libGlHandle, "glXGetProcAddressARB")) == nullptr)
	{
		printf("PipewireGL: failed to get glXGetProcAddressARB\n");
		return;
	}

	if ((glBindTexture = (glBindTextureFun)glXGetProcAddressARB("glBindTexture")) == nullptr)
	{
		printf("PipewireGL: failed to get glBindTexture\n");
		return;
	}

	if ((glDeleteTextures = (glDeleteTexturesFun)glXGetProcAddressARB("glDeleteTextures")) == nullptr)
	{
		printf("PipewireGL: failed to get glDeleteTextures\n");
		return;
	}

	if ((glGenTextures = (glGenTexturesFun)glXGetProcAddressARB("glGenTextures")) == nullptr)
	{
		printf("PipewireGL: failed to get glGenTextures\n");
		return;
	}

	if ((glGetError = (glGetErrorFun)glXGetProcAddressARB("glGetError")) == nullptr)
	{
		printf("PipewireGL: failed to get glGetError\n");
		return;
	}

	if ((glGetTexImage = (glGetTexImageFun)glXGetProcAddressARB("glGetTexImage")) == nullptr)
	{
		printf("PipewireGL: failed to get glGetTexImage\n");
		return;
	}

	if ((glTexParameteri = (glTexParameteriFun)glXGetProcAddressARB("glTexParameteri")) == nullptr)
	{
		printf("PipewireGL: failed to get glTexParameteri\n");
		return;
	}

	if (displayEgl == EGL_NO_DISPLAY)
	{
		bool x11session = qgetenv("XDG_SESSION_TYPE") == QByteArrayLiteral("x11") && !qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
		printf("Session type: %s , X11 detected: %s\n", qgetenv("XDG_SESSION_TYPE").constData(), (x11session) ? "yes" : "no");

		displayEgl = (x11session) ? eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, (void*)EGL_DEFAULT_DISPLAY, nullptr) :
									eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, (void*)EGL_DEFAULT_DISPLAY, nullptr);

		if (displayEgl == EGL_NO_DISPLAY)
		{
			printf("PipewireEGL: no EGL display\n");
			return;
		}
		else
		{
			EGLint major, minor;
			if (eglInitialize(displayEgl, &major, &minor))
			{
				printf("PipewireEGL: EGL initialized for HyperHDR. Version: %d.%d\n", major, minor);
			}
			else
			{
				printf("PipewireEGL: failed to init the display\n");
				return;
			}
		}
	}

	if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
	{
		printf("PipewireEGL: could not bing OpenGL API (reason = '%s')\n", eglErrorToString(eglGetError()));
		return;
	}

	if (contextEgl == EGL_NO_CONTEXT)
	{
		if ((contextEgl = eglCreateContext(displayEgl, nullptr, EGL_NO_CONTEXT, nullptr)) == EGL_NO_CONTEXT)
		{
			printf("PipewireEGL: Failed to create a context (reason = '%s')\n", eglErrorToString(eglGetError()));
			return;
		}
	}

	EGLint dmaCount = 0;

	if (!eglQueryDmaBufFormatsEXT(displayEgl, 0, nullptr, &dmaCount))
	{
		printf("PipewireEGL: Failed to query DMA-BUF format count (count = %d, reason = '%s')\n", dmaCount, eglErrorToString(eglGetError()));
		return;
	}
	else if (dmaCount > 0)
	{
		printf("PipewireEGL: Found %d DMA-BUF formats\n", dmaCount);
	}

	
	QVector<uint32_t> dmaFormats(dmaCount);

	if (dmaCount > 0 && eglQueryDmaBufFormatsEXT(displayEgl, dmaCount, (EGLint*)dmaFormats.data(), &dmaCount))
	{
		printf("PipewireEGL: got DMA format list (count = %d)\n", dmaCount);
	}
	else
	{
		printf("PipewireEGL: Failed to get DMA-BUF formats (reason = '%s')\n", eglErrorToString(eglGetError()));
		return;
	}


	foreach(const uint32_t& val, dmaFormats)
	{
		bool found = false;

		for(supportedDmaFormat& supVal : _supportedDmaFormatsList)
			if (val == supVal.drmFormat)
			{
				EGLint modCount = 0;
				if (eglQueryDmaBufModifiersEXT(displayEgl, supVal.drmFormat, 0, nullptr, nullptr, &modCount) && modCount > 0)
				{
					supVal.modifiers = QVector<uint64_t>(modCount);
					if (eglQueryDmaBufModifiersEXT(displayEgl, supVal.drmFormat, modCount, supVal.modifiers.data(), nullptr, &modCount))
					{
						printf("PipewireEGL: Found %s DMA format (%s)\n", supVal.friendlyName, fourCCtoString(val).toLocal8Bit().constData());
						supVal.hasDma = true;
						found = true;
						_initEGL = true;
						break;
					}
				}
			}

		if (!found)
		{
			printf("PipewireEGL: Found unsupported by HyperHDR '%s' DMA format\n", fourCCtoString(val).toLocal8Bit().constData());
		}
	}	
}
#endif

const QString PipewireHandler::fourCCtoString(int64_t val)
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
		.process = [](void *handler) {
			emit reinterpret_cast<PipewireHandler*>(handler)->onProcessFrameSignal();
		},
	};

	static const pw_core_events pwCoreEvents = {
		.version = PW_VERSION_CORE_EVENTS,
		.info = [](void *user_data, const struct pw_core_info *info) {
			std::cout << "Pipewire: core info reported. Version = " << info->version << std::endl;
		},		
		.error = [](void *handler, uint32_t id, int seq, int res, const char *message) {
			std::cout << "Pipewire: core error reported" << std::endl;
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
		const spa_pod*	streamParams[(sizeof(_supportedDmaFormatsList) / sizeof(supportedDmaFormat)) + 1];
		int streamParamsIndex = 0;
		
		MemoryBuffer<uint8_t> spaBufferMem(2048);

		auto spaBuilder = SPA_POD_BUILDER_INIT(spaBufferMem.data(), static_cast<uint32_t>(spaBufferMem.size()));

		#ifdef ENABLE_PIPEWIRE_EGL

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
				streamParams[streamParamsIndex++] = reinterpret_cast<spa_pod*> (spa_pod_builder_pop(&spaBuilder, &frameDMA[0]));
			}
		#endif

		streamParams[streamParamsIndex++] = reinterpret_cast<spa_pod*> (spa_pod_builder_add_object(&spaBuilder,
			SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
			SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
			SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
			SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(5,
				SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA,
				SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
			SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&pwScreenBoundsDefault, &pwScreenBoundsMin, &pwScreenBoundsMax),
			SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&pwFramerateDefault, &pwFramerateMin, &pwFramerateMax)));

		pw_stream_add_listener(stream, &_pwStreamListener, &pwStreamEvents, this);

		auto res = pw_stream_connect(stream, PW_DIRECTION_INPUT, _streamNodeId, static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), streamParams, streamParamsIndex);

		if ( res != 0)
		{
			printf("Pipewire: could not connect to the stream. Error code: %d\n", res);
			pw_stream_destroy(stream);
			stream = nullptr;
		}
		else
			printf("Pipewire: the stream is connected\n");
	}

	return stream;
}

