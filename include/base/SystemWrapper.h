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

class GlobalSignals;
class QTimer;

static QList<int> GRABBER_SYSTEM_CLIENTS;

class SystemWrapper : public QObject
{
	Q_OBJECT
public:
	SystemWrapper(const QString& grabberName, Grabber* ggrabber);

	~SystemWrapper() override;

	virtual bool isActivated(bool forced);

public slots:
	QJsonObject getJsonInfo();

	void newCapturedFrameHandler(const Image<ColorRgb>& image);
	void capturingExceptionHandler(const char* err);
	bool start();
	void stop();

private slots:
	void signalRequestSourceHandler(hyperhdr::Components component, int hyperHdrInd, bool listen);

signals:
	void SignalSystemImage(const QString& name, const Image<ColorRgb>& image);

public slots:
	void setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold);
	void setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax);
	void setSignalDetectionEnable(bool enable);
	void setDeviceVideoStandard(const QString& device);
	void setHdrToneMappingEnabled(int mode);
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	virtual void stateChanged(bool state);

protected:
	virtual QString getGrabberInfo();
	QString		_grabberName;
	Logger*		_log;
	bool		_configLoaded;
	Grabber*	_grabber;
};
