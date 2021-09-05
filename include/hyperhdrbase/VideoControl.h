#pragma once

#include <utils/Logger.h>
#include <utils/settings.h>
#include <utils/Components.h>
#include <utils/Image.h>
#include <QTimer>

class HyperHdrInstance;

///
/// @brief Capture Control class which is a interface to the HyperHdrDaemon native capture classes.
/// It controls the instance based enable/disable of capture feeds and PriorityMuxer registrations
///
class VideoControl : public QObject
{
	Q_OBJECT
public:
	VideoControl(HyperHdrInstance* hyperhdr);
		
	quint8 getCapturePriority()
	{
		return _usbCaptPrio;
	}

	bool isCEC();

signals:
	void setUsbCaptureEnableSignal(bool enable);

private slots:
	void setUsbCaptureEnable(bool enable);
	///
	/// @brief Handle component state change of V4L and SystemCapture
	/// @param component  The component from enum
	/// @param enable     The new state
	///
	void handleCompStateChangeRequest(hyperhdr::Components component, bool enable);

	///
	/// @brief Handle settings update from HyperHdr Settingsmanager emit or this constructor
	/// @param type   settingyType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);	

	///
	/// @brief forward v4l image
	/// @param image  The image
	///
	void handleUsbImage(const QString& name, const Image<ColorRgb> & image);

	///
	/// @brief Is called from _v4lInactiveTimer to set source after specific time to inactive
	///
	void setUsbInactive();	

private:
	/// HyperHdr instance
	HyperHdrInstance* _hyperhdr;	

	bool	_usbCaptEnabled;
	bool	_alive;
	quint8	_usbCaptPrio;
	QString	_usbCaptName;
	QTimer	_usbInactiveTimer;
	bool	_isCEC;

	static bool	_stream;
};
