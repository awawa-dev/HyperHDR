#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <QString>
	#include <QTimer>	
#endif

#include <utils/Components.h>
#include <api/CallbackAPI.h>
#include <base/HyperHdrInstance.h>
#include <base/HyperHdrManager.h>

class QNetworkReply;

class HyperAPI : public CallbackAPI
{
	Q_OBJECT

public:
	HyperAPI(QString peerAddress, Logger* log, bool localConnection, QObject* parent, bool noListener = false);
	void handleMessage(const QString& message, const QString& httpAuthHeader = "");
	void initialize();

public slots:
	void streamLedcolorsUpdate(const std::vector<ColorRgb>& ledColors);
	void incommingLogMessage(const Logger::T_LOG_MESSAGE&);
	hyperhdr::Components getActiveComponent();

private slots:
	void newPendingTokenRequest(const QString& id, const QString& comment);
	void handleTokenResponse(bool success, const QString& token, const QString& comment, const QString& id, const int& tan);
	void handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name = QString());	
	void handleLedColorsTimer();
	void lutDownloaded(QNetworkReply* reply, int hardware_brightness, int hardware_contrast, int hardware_saturation, qint64 time);

protected slots:
	void handleIncomingColors(const std::vector<ColorRgb>& ledValues) override;
	void handlerInstanceImageUpdated(const Image<ColorRgb>& image) override;
	void sendImage();

signals:
	void SignalCallbackBinaryImageMessage(Image<ColorRgb>);	
	void SignalForwardJsonMessage(QJsonObject);
	void SignalCallbackJsonMessage(QJsonObject);

protected:
	void stopDataConnections() override;

private:
	std::shared_ptr<LoggerManager> _logsManager;	

	bool _noListener;

	/// The peer address of the client
	QString _peerAddress;

	// streaming buffers
	QJsonObject _streaming_leds_reply;
	QJsonObject _streaming_image_reply;
	QJsonObject _streaming_logging_reply;

	/// flag to determine state of log streaming
	bool _streaming_logging_activated;

	/// timer for led color refresh
	QTimer* _ledStreamTimer;

	/// led stream refresh interval
	qint64 _colorsStreamingInterval;
	qint64 _lastSentImage;

	/// the current streaming led values
	std::vector<ColorRgb> _currentLedValues;

	bool handleInstanceSwitch(quint8 instance = 0, bool forced = false);
	void handleColorCommand(const QJsonObject& message, const QString& command, int tan);
	void handleImageCommand(const QJsonObject& message, const QString& command, int tan);
	void handleEffectCommand(const QJsonObject& message, const QString& command, int tan);
	void handleSysInfoCommand(const QJsonObject& message, const QString& command, int tan);
	void handleServerInfoCommand(const QJsonObject& message, const QString& command, int tan);
	void handleClearCommand(const QJsonObject& message, const QString& command, int tan);
	void handleClearallCommand(const QJsonObject& message, const QString& command, int tan);
	void handleAdjustmentCommand(const QJsonObject& message, const QString& command, int tan);
	void handleSourceSelectCommand(const QJsonObject& message, const QString& command, int tan);
	void handleConfigCommand(const QJsonObject& message, const QString& command, int tan);
	void handleSchemaGetCommand(const QJsonObject& message, const QString& command, int tan);
	void handleConfigSetCommand(const QJsonObject& message, const QString& command, int tan);
	void handleComponentStateCommand(const QJsonObject& message, const QString& command, int tan);
	void handleLedColorsCommand(const QJsonObject& message, const QString& command, int tan);
	void handleLoggingCommand(const QJsonObject& message, const QString& command, int tan);
	void handleProcessingCommand(const QJsonObject& message, const QString& command, int tan);
	void handleVideoModeHdrCommand(const QJsonObject& message, const QString& command, int tan);
	void handleLutCalibrationCommand(const QJsonObject& message, const QString& command, int tan);
	void handleAuthorizeCommand(const QJsonObject& message, const QString& command, int tan);
	void handleInstanceCommand(const QJsonObject& message, const QString& command, int tan);
	void handleLedDeviceCommand(const QJsonObject& message, const QString& command, int tan);
	void handleHelpCommand(const QJsonObject& message, const QString& command, int tan);
	void handleCropCommand(const QJsonObject& message, const QString& command, int tan);
	void handleVideoControlsCommand(const QJsonObject& message, const QString& command, int tan);
	void handleBenchmarkCommand(const QJsonObject& message, const QString& command, int tan);
	void handleLutInstallCommand(const QJsonObject& message, const QString& command, int tan);
	void handleSmoothingCommand(const QJsonObject& message, const QString& command, int tan);
	void handleCurrentStateCommand(const QJsonObject& message, const QString& command, int tan);
	void handleTunnel(const QJsonObject& message, const QString& command, int tan);
	void handleNotImplemented(const QString& command, int tan);
	void handleSaveDB(const QJsonObject& message, const QString& command, int tan);
	void handleLoadDB(const QJsonObject& message, const QString& command, int tan);
	void handleLoadSignalCalibration(const QJsonObject& message, const QString& command, int tan);
	void handlePerformanceCounters(const QJsonObject& message, const QString& command, int tan);

	void sendSuccessReply(const QString& command = "", int tan = 0);
	void sendSuccessDataReply(const QJsonDocument& doc, const QString& command = "", int tan = 0);
	void sendErrorReply(const QString& error, const QString& command = "", int tan = 0);

	bool isLocal(QString hostname);
};
