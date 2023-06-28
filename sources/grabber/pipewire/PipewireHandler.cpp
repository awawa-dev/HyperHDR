/* PipewireHandler.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDBusObjectPath>
#include <QObject>
#include <QFlags>
#include <QString>
#include <QVariantMap>
#include <QDebug>
#include <QDBusInterface>
#include <QUuid>
#include <QDBusContext>

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

#include <grabber/smartPipewire.h>
#include "PipewireHandler.h"

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

PipewireHandler::PipewireHandler() :	_sessionHandle(""), _restorationToken(""), _errorMessage(""), _portalStatus(false), _isError(false), _version(0), _streamNodeId(0),
									_sender(""), _replySessionPath(""), _sourceReplyPath(""), _startReplyPath(""),
									_pwMainThreadLoop(nullptr), _pwNewContext(nullptr), _pwContextConnection(nullptr), _pwStream(nullptr),
									_backupFrame(nullptr), _workingFrame(nullptr),
									_frameWidth(0),_frameHeight(0),_frameOrderRgb(false), _framePaused(false)
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
		if (_backupFrame != nullptr)
		{
			pw_stream_queue_buffer(_pwStream, _backupFrame);
			_backupFrame = nullptr;
		}

		if (_workingFrame != nullptr)
		{
			pw_stream_queue_buffer(_pwStream, _workingFrame);
			_workingFrame = nullptr;
		}

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

	if (_startReplyPath != "")
	{
		if (!QDBusConnection::sessionBus().disconnect(QString(), _startReplyPath, PORTAL_REQUEST, PORTAL_RESPONSE, this, SLOT(startResponse(uint, QVariantMap))))
			reportError("Failed to disconnect Start");
		_startReplyPath = "";
	}

	if (_sourceReplyPath != "")
	{
		if (!QDBusConnection::sessionBus().disconnect(QString(), _sourceReplyPath, PORTAL_REQUEST, PORTAL_RESPONSE, this, SLOT(selectSourcesResponse(uint, QVariantMap))))
			reportError("Failed to disconnect Source");
		_sourceReplyPath = "";
	}

	if (_replySessionPath != "")
	{
		if (!QDBusConnection::sessionBus().disconnect(QString(), _replySessionPath, PORTAL_REQUEST, PORTAL_RESPONSE, this, SLOT(createSessionResponse(uint, QVariantMap))))
			reportError("Failed to disconnect Session");
		_replySessionPath = "";
	}

	if (_sessionHandle != "")
	{
		QDBusMessage message = QDBusMessage::createMethodCall(DESKTOP_SERVICE, _sessionHandle, PORTAL_SESSION, QStringLiteral("Close"));
		QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
		pendingCall.waitForFinished();

		QDBusMessage reply = pendingCall.reply();
		if (reply.type() != QDBusMessage::ReplyMessage)
			reportError(QString("Pipewire: Failed to close the session. Error: %1 (%2)").arg(reply.errorMessage()).arg(reply.type()));

		std::cout << "Pipewire: handle (" << qPrintable(_sessionHandle) << ") released" << std::endl;

		_sessionHandle = "";
	}

	_pwStreamListener = {};
	_pwCoreListener = {};	
	_portalStatus = false;
	_isError = false;
	_errorMessage = "";
	_streamNodeId = 0;
	_frameWidth = 0;
	_frameHeight = 0;
	_frameOrderRgb = false;
	_backupFrame = nullptr;
	_workingFrame = nullptr;
	_framePaused = false;

	if (_version > 0)
	{
		std::cout << "Pipewire: driver is closed now" << std::endl;
		_version = 0;
	}
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

	QDBusInterface  iface(DESKTOP_SERVICE, DESKTOP_PATH, DESKTOP_SCREENCAST);

	if (iface.property("version").isValid())
	{
		version = iface.property("version").toInt();
		std::cout << "PipewireHandler: ScreenCast protocol version: " << qPrintable(QString("%1").arg(version)) << std::endl;
	}

	return version;
}

void PipewireHandler::startSession(QString restorationToken)
{
	std::cout << "Pipewire: initialization invoked. Cleaning up first..." << std::endl;

	closeSession();

	_restorationToken = QString("%1").arg(restorationToken);

	_version = PipewireHandler::readVersion();

	if (_version < 0)
	{
		reportError("Pipewire: Couldn't read Portal.ScreenCast protocol version. Probably Portal is not installed.");
		return;
	}

	_sender = QString("%1").arg(QDBusConnection::sessionBus().baseService()).replace('.','_');
	if (_sender.length() > 0 && _sender[0] == ':')
		_sender = _sender.right(_sender.length()-1);

	std::cout << "Sender: " << qPrintable(_sender) << std::endl;

	QString requestUUID = getRequestToken();

	_replySessionPath = QString(REQUEST_TEMPLATE).arg(_sender).arg(requestUUID);

	if (!QDBusConnection::sessionBus().connect(QString(), _replySessionPath, PORTAL_REQUEST, PORTAL_RESPONSE, this, SLOT(createSessionResponse(uint, QVariantMap))))
	{
		reportError(QString("Pipewire: can not add listener for CreateSession request (path: %1)").arg(_replySessionPath));
		_replySessionPath = "";
		return;
	}

	QDBusMessage message = QDBusMessage::createMethodCall(DESKTOP_SERVICE, DESKTOP_PATH, DESKTOP_SCREENCAST, QStringLiteral("CreateSession"));

	message << QVariantMap{ { QStringLiteral("session_handle_token"), getSessionToken() }, { QStringLiteral("handle_token"), requestUUID } };

	QDBusPendingReply<QDBusObjectPath> replySession = QDBusConnection::sessionBus().call(message);

	if (replySession.isError())
	{
		reportError(QString("Pipewire: Couldn't get reply for session create. Error: %1").arg(replySession.error().message()));
	}	

	std::cout << "Pipewire: CreateSession finished" << std::endl;
}


void PipewireHandler::createSessionResponse(uint response, const QVariantMap& results)
{
	std::cout << "Pipewire: Got response from portal CreateSession" << std::endl;

	if (response != 0)
	{
		reportError(QString("Pipewire: Failed to create session: %1").arg(response));
		return;
	}

	QString requestUUID = getRequestToken();

	QDBusMessage message = QDBusMessage::createMethodCall(DESKTOP_SERVICE, DESKTOP_PATH, DESKTOP_SCREENCAST, QStringLiteral("SelectSources"));

	_sessionHandle = results.value(QStringLiteral("session_handle")).toString();

	QVariantMap params = { { QStringLiteral("multiple"), false},
						   { QStringLiteral("types"), (uint)1 },
						   { QStringLiteral("cursor_mode"), (uint)1 },
						   { QStringLiteral("handle_token"), requestUUID },
						   { QStringLiteral("persist_mode"), (uint)2 } };

	if (!_restorationToken.isEmpty())
	{
		params[QStringLiteral("restore_token")] = _restorationToken;
		std::cout << "Pipewire: Has restoration token: " << qPrintable(QString(_restorationToken).right(12)) << std::endl;
	}

	message << QVariant::fromValue(QDBusObjectPath(_sessionHandle)) << params;

	_sourceReplyPath = QString(REQUEST_TEMPLATE).arg(_sender).arg(requestUUID);

	if (!QDBusConnection::sessionBus().connect(QString(), _sourceReplyPath, PORTAL_REQUEST, PORTAL_RESPONSE, this, SLOT(selectSourcesResponse(uint, QVariantMap))))
	{
		reportError(QString("Pipewire: can not add listener for Select request (path: %1)").arg(_sourceReplyPath));
		_sourceReplyPath = "";
		return;
	}

	QDBusPendingReply<QDBusObjectPath> sourceReply = QDBusConnection::sessionBus().call(message);

	if (sourceReply.isError())
		reportError(QString("Pirewire: Couldn't get reply for source select. Error: %1").arg(sourceReply.error().message()));

	std::cout << "Pipewire: SelectSources finished" << std::endl;
}


void PipewireHandler::selectSourcesResponse(uint response, const QVariantMap& results)
{
	Q_UNUSED(results);

	std::cout << "Pipewire: Got response from portal SelectSources" << std::endl;

	if (response != 0) {
		reportError(QString("Pipewire: Failed to select sources: %1").arg(response));
		return;
	}

	QString requestUUID = getRequestToken();

	QDBusMessage message = QDBusMessage::createMethodCall(DESKTOP_SERVICE, DESKTOP_PATH, DESKTOP_SCREENCAST, QStringLiteral("Start"));

	message << QVariant::fromValue(QDBusObjectPath(_sessionHandle))
		<< QString()
		<< QVariantMap{ { QStringLiteral("handle_token"), requestUUID } };

	_startReplyPath = QString(REQUEST_TEMPLATE).arg(_sender).arg(requestUUID);

	if (!QDBusConnection::sessionBus().connect(QString(), _startReplyPath, PORTAL_REQUEST, PORTAL_RESPONSE, this, SLOT(startResponse(uint, QVariantMap))))
	{
		reportError(QString("Pipewire: can not add listener for Start request (path: %1)").arg(_startReplyPath));
		_startReplyPath = "";
		return;
	}

	QDBusPendingReply<QDBusObjectPath> startReply = QDBusConnection::sessionBus().call(message);

	if (startReply.isError())
		reportError(QString("Pipewire: Couldn't get reply for start request. Error: %1").arg(startReply.error().message()));
	
	std::cout << "Pipewire: Start finished" << std::endl;
}

const QDBusArgument &operator >> (const QDBusArgument &arg, PipewireHandler::PipewireStructure &result)
{
	arg.beginStructure();
	arg >> result.objectId;

	result.width = 0;
	result.height = 0;

	arg.beginMap();
	for (struct { QString key; QVariant item; } input; !arg.atEnd(); result.properties.insert(input.key, input.item))
	{
		arg.beginMapEntry();
		arg >> input.key;

		if (input.key == "size" && arg.currentType() == QDBusArgument::VariantType)
		{
			arg.beginStructure();
				arg.beginMap();
				arg >> result.width >> result.height;
				std::cout << "Pipewire: format property size " << result.width << " x " << result.height << std::endl;
				arg.endMapEntry();
			arg.endStructure();
		}
		else
		{
			arg >> input.item;
			if (input.key != "position")
				std::cout << "Pipewire: format property " << qPrintable(input.key) << " = " << qPrintable(input.item.toString()) << std::endl;
		}
			
		arg.endMapEntry();
	}
	arg.endMap();
	arg.endStructure();

	return arg;
}

void PipewireHandler::startResponse(uint response, const QVariantMap& results)
{
	Q_UNUSED(results);

	std::cout << "Pipewire: Got response from portal Start" << std::endl;

	if (response != 0)
	{		
		reportError(QString("Pipewire: Failed to start or cancel dialog: %1").arg(response));
		_sessionHandle = "";
		return;
	}

	if (results.contains(QStringLiteral("restore_token")))
	{
		_restorationToken = qdbus_cast<QString>(results.value(QStringLiteral("restore_token")));
		std::cout << "Received restoration token: " << qPrintable(QString(_restorationToken).right(12)) << std::endl;
	}
	else
		std::cout << "No restoration token (portal protocol version 4 required and must be implemented by the backend GNOME/KDE... etc)" << std::endl;

	if (!results.contains(QStringLiteral("streams")))
	{
		reportError(QStringLiteral("Pipewire: no streams returned"));
		return;
	}

	auto streamsData = results.value(QStringLiteral("streams"));

	if (!streamsData.canConvert<QDBusArgument>())
	{
		reportError(QStringLiteral("Pipewire: invalid streams response"));
		return;
	}
	
	if (streamsData.value<QDBusArgument>().currentType() != QDBusArgument::ArrayType)
	{
		reportError(QStringLiteral("Pipewire: streams is not an array"));
		return;
	}

	QList<PipewireStructure> streamHandle;

	streamHandle = qdbus_cast<QList<PipewireStructure>>(streamsData);	
	_streamNodeId = streamHandle.first().objectId;
	_frameWidth = streamHandle.first().width;
	_frameHeight = streamHandle.first().height;
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
		if (_backupFrame != nullptr)
		{
			pw_stream_queue_buffer(_pwStream, _backupFrame);
			_backupFrame = nullptr;
		}

		if (_workingFrame != nullptr)
		{
			pw_stream_queue_buffer(_pwStream, _workingFrame);
			_workingFrame = nullptr;
		}

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
	if (state == PW_STREAM_STATE_CONNECTING) printf("Pipewire: state CONNECTING (%d, %d)\n", (int)state, (int)old);
	if (state == PW_STREAM_STATE_PAUSED) printf("Pipewire: state PAUSED (%d, %d)\n", (int)state, (int)old);
	if (state == PW_STREAM_STATE_STREAMING) printf("Pipewire: state STREAMING (%d, %d)\n", (int)state, (int)old);
}

void PipewireHandler::onParamsChanged(uint32_t id, const struct spa_pod* param)
{
	struct spa_video_info format;

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
	_frameOrderRgb = (format.info.raw.format == SPA_VIDEO_FORMAT_RGBx || format.info.raw.format == SPA_VIDEO_FORMAT_RGBA);

	printf("Pipewire: video format = %d (%s)\n", format.info.raw.format, spa_debug_type_find_name(spa_type_video_format, format.info.raw.format));
	printf("Pipewire: video size = %dx%d (RGB order = %s)\n", _frameWidth, _frameHeight, (_frameOrderRgb) ? "true" : "false");
	printf("Pipewire: framerate = %d/%d\n", format.info.raw.framerate.num, format.info.raw.framerate.denom);	
};

void PipewireHandler::onProcessFrame()
{
	struct pw_buffer* newFrame;

	if ((newFrame = pw_stream_dequeue_buffer(_pwStream)) == nullptr)
	{
		std::cout << "Pipewire: out of buffers" << std::endl;
		return;
	}

	if (newFrame->buffer->datas[0].data == nullptr)
	{
		std::cout << "Pipewire: empty buffer" << std::endl;
		return;
	}

	if (_backupFrame != nullptr)
		pw_stream_queue_buffer(_pwStream, _backupFrame);

	_backupFrame = newFrame;	
};

void PipewireHandler::onCoreError(uint32_t id, int seq, int res, const char *message)
{
	reportError(QString("Pipewire: core reports error '%1'").arg(message));
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
	spa_fraction pwFramerateDefault = SPA_FRACTION(25, 1);
	spa_fraction pwFramerateMax = SPA_FRACTION(60, 1);

	pw_properties* reuseProps = pw_properties_new_string("pipewire.client.reuse=1");
	
	pw_core_add_listener(_pwContextConnection, &_pwCoreListener, &pwCoreEvents, this);

	pw_stream* stream = pw_stream_new(_pwContextConnection, "hyperhdr-stream-receiver", reuseProps);

	if (stream != nullptr)
	{
		const int spaBufferSize = 1024;
		const spa_pod*	streamParams[1];

		uint8_t* spaBuffer = static_cast<uint8_t*>(calloc(spaBufferSize, 1));

		auto spaBuilder = SPA_POD_BUILDER_INIT(spaBuffer, spaBufferSize);

		streamParams[0] = reinterpret_cast<spa_pod*> (spa_pod_builder_add_object(&spaBuilder,
											   SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
											   SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
											   SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
											   SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(5,
													  SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA,
													  SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
											   SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle( &pwScreenBoundsDefault, &pwScreenBoundsMin, &pwScreenBoundsMax),
											   SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction( &pwFramerateDefault, &pwFramerateMin, &pwFramerateMax)));		

		pw_stream_add_listener(stream, &_pwStreamListener, &pwStreamEvents, this);

		if (pw_stream_connect(stream, PW_DIRECTION_INPUT, _streamNodeId, static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), streamParams, 1) != 0)
		{
			pw_stream_destroy(stream);
			stream = nullptr;
		}

		free(spaBuffer);
	}

	return stream;
}

void PipewireHandler::getImage(PipewireImage* image)
{
	image->version = getVersion();
	image->isError = hasError();
	image->data = nullptr;
	if (_workingFrame == nullptr && _backupFrame != nullptr)
	{
		if (static_cast<int>(_backupFrame->buffer->datas[0].chunk->size) == (_frameWidth * _frameHeight * 4))
		{
			_workingFrame = _backupFrame;
			_backupFrame = nullptr;

			image->width = _frameWidth;
			image->height = _frameHeight;
			image->isOrderRgb = _frameOrderRgb;

			image->data = static_cast<unsigned char*>(_workingFrame->buffer->datas[0].data);
		}
		else
			printf("Pipewire: unexpected frame size. Got: %d, expected: %d\n", _backupFrame->buffer->datas[0].chunk->size, (_frameWidth * _frameHeight * 4));
	}

	if (_framePaused && _pwStream != nullptr)
		pw_stream_set_active(_pwStream, true);
}

void PipewireHandler::releaseWorkingFrame()
{
	if (_pwStream == nullptr || _workingFrame == nullptr)
		return;

	if (_backupFrame == nullptr)
	{
		_backupFrame = _workingFrame;
	}
	else	
	{
		pw_stream_queue_buffer(_pwStream, _workingFrame);		
	}

	_workingFrame = nullptr;
}
