#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QWidget>
#include <QColorDialog>
#include <QCloseEvent>

#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/HyperHdrIManager.h>

class HyperHdrDaemon;

class SysTray : public QWidget
{
	Q_OBJECT

public:
	SysTray(HyperHdrDaemon *hyperiond);
	~SysTray();


public slots:
	void showColorDialog();
	void setColor(const QColor & color);
	void closeEvent(QCloseEvent *event);
	void settings();
	void setEffect();
	void clearEfxColor();
	void setAutorunState();

private slots:
	void iconActivated(QSystemTrayIcon::ActivationReason reason);

	///
	/// @brief is called whenever the webserver changes the port
	///
	void webserverPortChanged(quint16 port) { _webPort = port; };

	///
	/// @brief is called whenever a hyperion instance state changes
	///
	void handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name);

private:
	void createTrayIcon();

#ifdef _WIN32
	///
	/// @brief Checks whether Hyperion should start at Windows system start.
	/// @return True on success, otherwise false
	///
	bool getCurrentAutorunState();
#endif

	QAction          *quitAction;
	QAction          *startAction;
	QAction          *stopAction;
	QAction          *colorAction;
	QAction          *settingsAction;
	QAction          *clearAction;
#ifdef _WIN32
	QAction          *autorunAction;
#endif

	QSystemTrayIcon  *_trayIcon;
	QMenu            *_trayIconMenu;
	QMenu            *_trayIconEfxMenu;
	QColorDialog      _colorDlg;
	HyperHdrDaemon   *_hyperiond;
	HyperHdrInstance *_hyperion;
	HyperHdrIManager *_instanceManager;
	quint16           _webPort;
};
