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

class Grabber;
class GlobalSignals;
class QTimer;

/// List of HyperHDR instances that requested screen capt
static QList<int> GRABBER_SYS_CLIENTS;
static QList<int> GRABBER_V4L_CLIENTS;

///
/// This class will be inherted by FramebufferWrapper and others which contains the real capture interface
///
class GrabberWrapper : public QObject
{
	Q_OBJECT
public:
	GrabberWrapper(const QString& grabberName, Grabber * ggrabber, unsigned width, unsigned height, unsigned updateRate_Hz = 0);

	~GrabberWrapper() override;

	static GrabberWrapper* instance;
	static GrabberWrapper* getInstance(){ return instance; }
	void setHdrToneMappingEnabled(int mode);
	///
	/// Starts the grabber which produces led values with the specified update rate
	///
	virtual bool start();

	///
	/// Starts maybe the grabber which produces led values with the specified update rate
	///
	virtual void tryStart();

	///
	/// Stop grabber
	///
	virtual void stop();

	///
	/// Check if grabber is active
	///
	virtual bool isActive() const;

	///
	/// @brief Get a list of all available V4L devices
	/// @return List of all available V4L devices on success else empty List
	///
	virtual QStringList getV4L2devices() const;

	///
	/// @brief Get the V4L device name
	/// @param devicePath The device path
	/// @return The name of the V4L device on success else empty String
	///
	virtual QString getV4L2deviceName(const QString& devicePath) const;

	///
	/// @brief Get a name/index pair of supported device inputs
	/// @param devicePath The device path
	/// @return multi pair of name/index on success else empty pair
	///
	virtual QMultiMap<QString, int> getV4L2deviceInputs(const QString& devicePath) const;

	///
	/// @brief Get a list of supported device resolutions
	/// @param devicePath The device path
	/// @return List of resolutions on success else empty List
	///
	virtual QStringList getResolutions(const QString& devicePath) const;

	///
	/// @brief Get a list of supported device framerates
	/// @param devicePath The device path
	/// @return List of framerates on success else empty List
	///
	virtual QStringList getFramerates(const QString& devicePath) const;

	virtual QStringList getVideoCodecs(const QString& devicePath) const;

	///
	/// @brief Get active grabber name
	/// @return Active grabber name
	///
	virtual QString getActive() const;

	static QStringList availableGrabbers();

public slots:
	///
	/// virtual method, should perform single frame grab and computes the led-colors
	///
	virtual void action() = 0;


	///
	/// Set the crop values
	/// @param  cropLeft    Left pixel crop
	/// @param  cropRight   Right pixel crop
	/// @param  cropTop     Top pixel crop
	/// @param  cropBottom  Bottom pixel crop
	///
	virtual void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom);

	virtual void handleSettingsUpdate(settings::type type, const QJsonDocument& config) = 0;

signals:
	///
	/// @brief Emit the final processed image
	///
	void systemImage(const QString& name, const Image<ColorRgb>& image);
	void HdrChanged(int mode);

private slots:
	/// @brief Handle a source request event from HyperHDR.
	/// Will start and stop grabber based on active listeners count
	void handleSourceRequest(hyperhdr::Components component, int hyperHdrInd, bool listen);

	///
	/// @brief Update Update capture rate
	/// @param type   interval between frames in millisecons
	///
	void updateTimer(int interval);

protected:
	QString _grabberName;

	/// The timer for generating events with the specified update rate
	QTimer* _timer;

	/// The calced update rate [ms]
	int _updateInterval_ms;

	/// The Logger instance
	Logger* _log;

	Grabber* _ggrabber;

	/// The image used for grabbing frames
	Image<ColorRgb> _image;
};
