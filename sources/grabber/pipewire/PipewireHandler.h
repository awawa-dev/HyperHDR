#include <QObject>
#include <QList>
#include <QVariantMap>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/utils/hook.h>
#include <spa/debug/types.h>

struct PipewireImage;

class PipewireHandler : public QObject
{
	Q_OBJECT

public:

	PipewireHandler();
	~PipewireHandler();
	void startSession(QString restorationToken);
	void closeSession();
	bool hasError();

	int		getVersion();
	QString getToken();
	QString getError();

	void getImage(PipewireImage* image);
	void releaseWorkingFrame();

	static int	readVersion();

	struct PipewireStructure
	{
		uint		objectId;
		uint		width, height;
		QVariantMap properties;
	};

public Q_SLOTS:

	void createSessionResponse(uint response, const QVariantMap& results);
	void selectSourcesResponse(uint response, const QVariantMap& results);
	void startResponse(uint response, const QVariantMap& results);

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
	void reportError(const QString& input);

	pw_stream*	createCapturingStream();
	QString		getSessionToken();
	QString		getRequestToken();

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
};
