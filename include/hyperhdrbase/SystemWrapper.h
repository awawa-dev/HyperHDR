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

class GlobalSignals;
class QTimer;

/// List of HyperHDR instances that requested screen capt
static QList<int> GRABBER_SYSTEM_CLIENTS;

class SystemWrapper : public QObject
{
	Q_OBJECT
public:
	SystemWrapper(const QString& grabberName, Grabber * ggrabber);

	~SystemWrapper() override;

	static	SystemWrapper* instance;
	static	SystemWrapper* getInstance(){ return instance; }	
	void tryStart();

	QStringList getVideoDevices() const;
	QString getVideoDeviceName(const QString& devicePath) const;
	QMap<Grabber::currentVideoModeInfo, QString> getVideoCurrentMode() const;

	QString getActive() const;	

	static QStringList availableGrabbers();

public slots:	
	void newFrame(const Image<ColorRgb>& image);
	void readError(const char* err);
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
	void StateChanged(QString device, QString videoMode);
	

public:
	int  getHdrToneMappingEnabled();

public slots:
	void setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold);
	void setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax);
	void setSignalDetectionEnable(bool enable);
	void setDeviceVideoStandard(const QString& device);	
	void setHdrToneMappingEnabled(int mode);
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

protected:
	QString		_grabberName;

	Logger*		_log;

	bool		_configLoaded;

	Grabber*	_grabber;
};
