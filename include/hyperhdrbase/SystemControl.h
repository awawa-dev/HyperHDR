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
class SystemControl : public QObject
{
	Q_OBJECT

public:
	SystemControl(HyperHdrInstance* hyperhdr);

	quint8 getCapturePriority()
	{
		return _sysCaptPrio;
	}

	bool isCEC();

signals:
	void setSysCaptureEnableSignal(bool enable);

private slots:
	void setSysCaptureEnable(bool enable);
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
	void handleSysImage(const QString& name, const Image<ColorRgb>& image);

	///
	/// @brief Is called from _v4lInactiveTimer to set source after specific time to inactive
	///
	void setSysInactive();

private:
	/// HyperHdr instance
	HyperHdrInstance* _hyperhdr;

	bool	_sysCaptEnabled;
	bool	_alive;
	quint8	_sysCaptPrio;
	QString	_sysCaptName;
	QTimer	_sysInactiveTimer;
	bool	_isCEC;
};
