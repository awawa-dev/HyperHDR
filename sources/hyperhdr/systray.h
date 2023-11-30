#pragma once

#ifndef PCH_ENABLED		
	#include <memory>
	#include <vector>
#endif

#include <base/HyperHdrInstance.h>
#include <base/HyperHdrManager.h>
#include <QSystemTrayIcon>
#include <QWidget>

class HyperHdrDaemon;
class QMenu;
class QAction;
class QColorDialog;


class SysTray : public QObject
{
	Q_OBJECT

public:
	SysTray(HyperHdrDaemon* hyperhdrDaemon, quint16 webPort);
	~SysTray();

public slots:
	void showColorDialog();
	void setColor(const QColor& color);
	void settings();
	void setEffect();
	void clearEfxColor();
	void setAutorunState();

private slots:
	void iconActivated(QSystemTrayIcon::ActivationReason reason);

	///
	/// @brief is called whenever a hyperhdr instance state changes
	///
	void signalInstanceStateChangedHandler(InstanceState state, quint8 instance, const QString& name);
	void signalSettingsChangedHandler(settings::type type, const QJsonDocument& data);

private:
	void createTrayIcon();

#ifdef _WIN32
	///
	/// @brief Checks whether HyperHDR should start at Windows system start.
	/// @return True on success, otherwise false
	///
	bool getCurrentAutorunState();
#endif

	QAction* _quitAction;
	QAction* _startAction;
	QAction* _stopAction;
	QAction* _colorAction;
	QAction* _settingsAction;
	QAction* _clearAction;
	QAction* _autorunAction;

	QSystemTrayIcon* _trayIcon;
	QMenu*           _trayIconMenu;
	QMenu*           _trayIconEfxMenu;

	QColorDialog*		_colorDlg;

	std::weak_ptr<HyperHdrManager> _instanceManager;
	std::weak_ptr<HyperHdrInstance>	_hyperhdrHandle;
	quint16				_webPort;
};
