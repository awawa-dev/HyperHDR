#pragma once

#include <utils/Logger.h>
#include <utils/settings.h>
#include <utils/Components.h>
#include <utils/Image.h>

class Hyperion;
class QTimer;

///
/// @brief Capture Control class which is a interface to the HyperionDaemon native capture classes.
/// It controls the instance based enable/disable of capture feeds and PriorityMuxer registrations
///
class CaptureCont : public QObject
{
	Q_OBJECT
public:
	CaptureCont(Hyperion* hyperion);
	
	void setV4LCaptureEnable(bool enable);

private slots:
	///
	/// @brief Handle component state change of V4L and SystemCapture
	/// @param component  The component from enum
	/// @param enable     The new state
	///
	void handleCompStateChangeRequest(hyperion::Components component, bool enable);

	///
	/// @brief Handle settings update from Hyperion Settingsmanager emit or this constructor
	/// @param type   settingyType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);	

	///
	/// @brief forward v4l image
	/// @param image  The image
	///
	void handleV4lImage(const QString& name, const Image<ColorRgb> & image);

	///
	/// @brief Is called from _v4lInactiveTimer to set source after specific time to inactive
	///
	void setV4lInactive();


private:
	/// Hyperion instance
	Hyperion* _hyperion;	

	/// Reflect state of v4l capture and prio
	bool _v4lCaptEnabled;
	quint8 _v4lCaptPrio;
	QString _v4lCaptName;
	QTimer* _v4lInactiveTimer;
};
