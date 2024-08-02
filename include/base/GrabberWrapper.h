#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QJsonObject>
	#include <QJsonArray>
	#include <QString>
	#include <QStringList>
	#include <QMultiMap>
#endif

#include <image/ColorRgb.h>
#include <image/Image.h>
#include <utils/Logger.h>
#include <utils/settings.h>
#include <utils/Components.h>
#include <base/Grabber.h>
#include <base/DetectionAutomatic.h>

class GlobalSignals;
class QTimer;

class GrabberWrapper : public QObject
{
	Q_OBJECT

public:
	GrabberWrapper(const QString& grabberName);

	~GrabberWrapper() override;

	bool isCEC();	
	bool getAutoResume();

public slots:
	void capturingExceptionHandler(const char* err);	

	bool start();
	void stop();
	void revive();

	void benchmarkCapture(int status, QString message);

	QJsonObject getJsonInfo();

	QJsonDocument startCalibration();
	QJsonDocument stopCalibration();
	QJsonDocument getCalibrationInfo();

private slots:
	void signalRequestSourceHandler(hyperhdr::Components component, int instanceIndex, bool listen);

signals:
	///
	/// @brief Emit the final processed image
	///
	void SignalNewVideoImage(const QString& name, const Image<ColorRgb>& image);
	void SignalVideoStreamChanged(QString device, QString videoMode);
	void SignalCecKeyPressed(int key);
	void SignalBenchmarkUpdate(int status, QString message);
	void SignalInstancePauseChanged(int instance, bool isEnabled);
	void SignalSetNewComponentStateToAllInstances(hyperhdr::Components component, bool enable);
	void SignalSaveCalibration(QString saveData);

public slots:
	void signalCecKeyPressedHandler(int key);
	int  getHdrToneMappingEnabled();
	void setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold);
	void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom);
	void setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax);
	void setSignalDetectionEnable(bool enable);
	void setDeviceVideoStandard(const QString& device);
	void setFpsSoftwareDecimation(int decimation);	
	void setEncoding(QString enc);
	void setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue);
	void setQFrameDecimation(int setQframe);
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void signalInstancePauseChangedHandler(int instance, bool isEnabled);
	QString getVideoCurrentModeResolution();

protected:
	void setHdrToneMappingEnabled(int mode);
	void setCecStartStop(int cecHdrStart, int cecHdrStop);

	QMap<Grabber::currentVideoModeInfo, QString> getVideoCurrentMode() const;
	DetectionAutomatic::calibrationPoint parsePoint(int width, int height, QJsonObject element, bool& ok);

	QString		_grabberName;

	Logger*		_log;

	bool		_configLoaded;

	std::unique_ptr<Grabber> _grabber;

	int			_cecHdrStart;
	int			_cecHdrStop;
	bool		_autoResume;
	bool		_isPaused;
	bool		_pausingModeEnabled;

	QList<int>	_running_clients;
	QList<int>	_paused_clients;
};
