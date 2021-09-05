#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QMultiMap>

#include <utils/Logger.h>
#include <utils/Components.h>
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/settings.h>
#include <hyperhdrbase/Grabber.h>
#include <hyperhdrbase/DetectionAutomatic.h>

class GlobalSignals;
class QTimer;

/// List of HyperHDR instances that requested screen capt
static QList<int> GRABBER_VIDEO_CLIENTS;

///
/// This class will be inherted by FramebufferWrapper and others which contains the real capture interface
///
class GrabberWrapper : public QObject
{
	Q_OBJECT
public:
	GrabberWrapper(const QString& grabberName, Grabber * ggrabber);

	~GrabberWrapper() override;

	static	GrabberWrapper* instance;
	static	GrabberWrapper* getInstance(){ return instance; }	

	void tryStart();

	QStringList getVideoDevices() const;

	QString getVideoDeviceName(const QString& devicePath) const;

	QMap<Grabber::currentVideoModeInfo, QString> getVideoCurrentMode() const;

	QMultiMap<QString, int> getVideoDeviceInputs(const QString& devicePath) const;

	QMap<Grabber::VideoControls, int> getVideoDeviceControls(const QString& devicePath);

	QList<Grabber::DevicePropertiesItem> getVideoDeviceModesFullInfo(const QString& devicePath);

	QString getActive() const;	

	static QStringList availableGrabbers();

	bool isCEC();

	void setCecStartStop(int cecHdrStart, int cecHdrStop);

	QJsonDocument startCalibration();

	QJsonDocument stopCalibration();

	QJsonDocument getCalibrationInfo();

	int getFpsSoftwareDecimation();

	int getActualFps();

	void revive();
	
public slots:	
	void newFrame(const Image<ColorRgb>& image);
	void readError(const char* err);
	void cecKeyPressedHandler(int key);

	bool start();
	void stop();

private slots:
	void handleSourceRequest(hyperhdr::Components component, int hyperHdrInd, bool listen);
	void action(){};

signals:
	///
	/// @brief Emit the final processed image
	///
	void systemImage(const QString& name, const Image<ColorRgb>& image);
	void HdrChanged(int mode);
	void StateChanged(QString device, QString videoMode);
	void cecKeyPressed(int key);
	

public:
	int  getHdrToneMappingEnabled();

public slots:
	void setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold);
	void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom);
	void setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax);
	void setSignalDetectionEnable(bool enable);
	void setDeviceVideoStandard(const QString& device);	
	void setFpsSoftwareDecimation(int decimation);
	void setHdrToneMappingEnabled(int mode);
	void setEncoding(QString enc);
	void setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue);
	void setQFrameDecimation(int setQframe);
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

protected:
	DetectionAutomatic::calibrationPoint parsePoint(int width, int height, QJsonObject element, bool& ok);

	QString		_grabberName;
	
	Logger*		_log;

	bool		_configLoaded;

	Grabber*	_grabber;

	int			_cecHdrStart;
	int			_cecHdrStop;
	bool		_autoResume;
};
