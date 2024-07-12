#pragma once

#include <QObject>
#include <QVector>
#include <QList>
#include <QTimer>
#include <QVariantMap>
#include <image/MemoryBuffer.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/utils/hook.h>
#include <spa/debug/types.h>
#include <grabber/linux/pipewire/smartPipewire.h>
#include <linux/types.h>
#include <HyperhdrConfig.h>
#include <memory>

#if !PW_CHECK_VERSION(0, 3, 29)
#define SPA_POD_PROP_FLAG_MANDATORY (1u << 3)
#endif
#if !PW_CHECK_VERSION(0, 3, 33)
#define SPA_POD_PROP_FLAG_DONT_FIXATE (1u << 4)
#endif

#ifdef ENABLE_PIPEWIRE_EGL
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef void* (*eglGetProcAddressFun)(const char*);
typedef EGLDisplay(*eglGetPlatformDisplayFun)(EGLenum platform, void* native_display, const EGLAttrib* attrib_list);
typedef EGLBoolean(*eglTerminateFun)(EGLDisplay display);
typedef EGLBoolean(*eglInitializeFun)(EGLDisplay dpy, EGLint* major, EGLint* minor);
typedef EGLint(*eglGetErrorFun)(void);
typedef EGLBoolean (*eglQueryDmaBufFormatsEXTFun)(EGLDisplay dpy, EGLint max_formats, EGLint* formats, EGLint* num_formats);
typedef EGLBoolean (*eglQueryDmaBufModifiersEXTFun)(EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR* modifiers, EGLBoolean* external_only, EGLint* num_modifiers);
typedef EGLImageKHR (*eglCreateImageKHRFun)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint* attrib_list);
typedef EGLBoolean (*eglDestroyImageKHRFun)(EGLDisplay dpy, EGLImageKHR image);
typedef EGLContext(*eglCreateContextFun)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list);
typedef EGLBoolean(*eglDestroyContextFun)(EGLDisplay display, EGLContext context);
typedef EGLBoolean(*eglMakeCurrentFun)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
typedef EGLBoolean(*eglBindAPIFun)(EGLenum api);
typedef void (*glEGLImageTargetTexture2DOESFun)(GLenum target, GLeglImageOES image);

typedef void* (*glXGetProcAddressARBFun)(const char*);
typedef void (*glBindTextureFun)(GLenum target, GLuint texture);
typedef void (*glDeleteTexturesFun)(GLsizei n, const GLuint* textures);
typedef void (*glGenTexturesFun)(GLsizei n, GLuint* textures);
typedef GLenum (*glGetErrorFun)(void);
typedef void (*glGetTexImageFun)(GLenum target, GLint level, GLenum format, GLenum type, void* pixels);
typedef void (*glTexParameteriFun)(GLenum target, GLenum pname, GLint param);

#define fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
				 ((__u32)(c) << 16) | ((__u32)(d) << 24))
#define fourcc_mod_code(vendor, val) \
	((((__u64)DRM_FORMAT_MOD_VENDOR_## vendor) << 56) | ((val) & 0x00ffffffffffffffULL))
#define DRM_FORMAT_MOD_VENDOR_NONE    0
#define DRM_FORMAT_RESERVED	      ((1ULL << 56) - 1)
#define DRM_FORMAT_MOD_INVALID	fourcc_mod_code(NONE, DRM_FORMAT_RESERVED)

#define DRM_FORMAT_XRGB8888	fourcc_code('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888	fourcc_code('X', 'B', '2', '4')
#define DRM_FORMAT_ARGB8888	fourcc_code('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888	fourcc_code('A', 'B', '2', '4')
#endif

namespace sdbus{
	class IConnection;
	class IProxy;
}
class ScreenCastProxy;

class PipewireHandler : public QObject
{
	Q_OBJECT

public:

	PipewireHandler();
	~PipewireHandler();
	void startSession(QString restorationToken, uint32_t requestedFPS);
	void closeSession();
	bool hasError();
	
	int		getVersion();
	QString getToken();
	QString getError();

	static int	readVersion();

	struct PipewireStructure
	{
		uint		objectId;
		uint		width, height;
		QVariantMap properties;
	};

public Q_SLOTS:
	void releaseWorkingFrame();
	void getImage(PipewireImage& retVal);
	void createSessionResponse(uint response, QString session);
	void selectSourcesResponse(uint response);
	void startResponse(uint response, QString restoreHandle, uint32_t nodeId, int nodeStreamWidth, int nodeStreamHeight);

	void onParamsChanged(uint32_t id, const struct spa_pod* param);
	void onStateChanged(enum pw_stream_state old, enum pw_stream_state state, const char* error);
	void onProcessFrame();
	void onCoreError(uint32_t id, int seq, int res, const char *message);
	
signals:
	void onParamsChangedSignal(uint32_t id, const struct spa_pod* param);
	void onStateChangedSignal(enum pw_stream_state old, enum pw_stream_state state, const char* error);
	void onProcessFrameSignal();
	void onCoreErrorSignal(uint32_t id, int seq, int res, const char *message);

private:
	uint8_t* createMemory(int size);
	void reportError(const QString& input);
	const QString fourCCtoString(int64_t val);

	pw_stream*	createCapturingStream();
	QString		getSessionToken();
	QString		getRequestToken();
	void		captureFrame();

	QString _sessionHandle;
	QString _restorationToken;
	QString _errorMessage;
	bool	_portalStatus;
	bool	_isError;
	int		_version;
	uint	_streamNodeId;

	QString _sender;
	QString _replySessionPath;
	QString _sourceReplyPath;
	QString _startReplyPath;

	struct pw_thread_loop*	_pwMainThreadLoop;
	struct pw_context*		_pwNewContext;
	struct pw_core*			_pwContextConnection;
	struct pw_stream*		_pwStream;
	struct spa_hook			_pwStreamListener;
	struct spa_hook			_pwCoreListener;
	struct pw_buffer*		_backupFrame;
	struct pw_buffer*		_workingFrame;

	int		_frameWidth;
	int		_frameHeight;
	bool	_frameOrderRgb;
	bool	_framePaused;
	uint32_t _requestedFPS;
	bool	_hasFrame;
	bool	_infoUpdated;
	bool	_initEGL;
	void*	_libEglHandle;
	void*	_libGlHandle;
	int64_t _frameDrmFormat;
	int64_t _frameDrmModifier;
	PipewireImage _image;

	MemoryBuffer<uint8_t> _memoryCache;

	std::unique_ptr<sdbus::IConnection> _dbusConnection;
	std::unique_ptr<ScreenCastProxy> _screenCastProxy;
	std::unique_ptr<sdbus::IProxy> _createSessionProxy;
	std::unique_ptr<sdbus::IProxy> _selectSourceProxy;
	std::unique_ptr<sdbus::IProxy> _startProxy;

#ifdef ENABLE_PIPEWIRE_EGL
	eglGetProcAddressFun eglGetProcAddress = nullptr;
	eglInitializeFun eglInitialize = nullptr;
	eglTerminateFun eglTerminate = nullptr;
	eglGetPlatformDisplayFun eglGetPlatformDisplay = nullptr;
	eglGetErrorFun eglGetError = nullptr;
	eglQueryDmaBufFormatsEXTFun eglQueryDmaBufFormatsEXT = nullptr;
	eglQueryDmaBufModifiersEXTFun eglQueryDmaBufModifiersEXT = nullptr;
	eglCreateImageKHRFun eglCreateImageKHR = nullptr;
	eglDestroyImageKHRFun eglDestroyImageKHR = nullptr;
	eglCreateContextFun eglCreateContext = nullptr;
	eglDestroyContextFun eglDestroyContext = nullptr;
	eglMakeCurrentFun eglMakeCurrent = nullptr;
	glEGLImageTargetTexture2DOESFun glEGLImageTargetTexture2DOES = nullptr;
	eglBindAPIFun eglBindAPI = nullptr;

	glXGetProcAddressARBFun glXGetProcAddressARB = nullptr;
	glBindTextureFun glBindTexture = nullptr;
	glDeleteTexturesFun glDeleteTextures = nullptr;
	glGenTexturesFun glGenTextures = nullptr;
	glGetErrorFun glGetError = nullptr;
	glGetTexImageFun glGetTexImage = nullptr;
	glTexParameteriFun glTexParameteri = nullptr;

	EGLDisplay displayEgl = EGL_NO_DISPLAY;
	EGLContext contextEgl = EGL_NO_CONTEXT;

	const char* eglErrorToString(EGLint error_number);
	const char* glErrorToString(GLenum errorType);
	void initEGL();

	struct supportedDmaFormat {
		int64_t  drmFormat;
		uint32_t spaFormat;
		GLenum glFormat;
		const char* friendlyName;
		bool hasDma;
		QVector<uint64_t> modifiers;		
	};

	supportedDmaFormat _supportedDmaFormatsList[4] = {						
			{ DRM_FORMAT_XRGB8888, SPA_VIDEO_FORMAT_BGRx, GL_BGRA, "DRM_FORMAT_XRGB8888", false},
			{ DRM_FORMAT_ARGB8888, SPA_VIDEO_FORMAT_BGRA, GL_BGRA, "DRM_FORMAT_ARGB8888", false},
			{ DRM_FORMAT_XBGR8888, SPA_VIDEO_FORMAT_RGBx, GL_RGBA, "DRM_FORMAT_XBGR8888", false},
			{ DRM_FORMAT_ABGR8888, SPA_VIDEO_FORMAT_RGBA, GL_RGBA, "DRM_FORMAT_ABGR8888", false}
	};

#endif

};
