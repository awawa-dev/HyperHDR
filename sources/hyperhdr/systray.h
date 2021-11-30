#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QWidget>
#include <QColorDialog>
#include <QCloseEvent>

#include <memory>
#include <vector>

#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/HyperHdrIManager.h>

class HyperHdrDaemon;

class SysTray : public QWidget
{
	Q_OBJECT

public:
	SysTray(HyperHdrDaemon* hyperhdrd);
	~SysTray();


public slots:
	void showColorDialog();
	void setColor(const QColor& color);
	void closeEvent(QCloseEvent* event);
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
	/// @brief is called whenever a hyperhdr instance state changes
	///
	void handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name);

private:
	void createTrayIcon();

#ifdef _WIN32
	///
	/// @brief Checks whether HyperHDR should start at Windows system start.
	/// @return True on success, otherwise false
	///
	bool getCurrentAutorunState();
#endif

	std::unique_ptr<QAction> _quitAction;
	std::unique_ptr<QAction> _startAction;
	std::unique_ptr<QAction> _stopAction;
	std::unique_ptr<QAction> _colorAction;
	std::unique_ptr<QAction> _settingsAction;
	std::unique_ptr<QAction> _clearAction;
	std::unique_ptr<QAction> _autorunAction;

	std::unique_ptr<QSystemTrayIcon> _trayIcon;
	std::unique_ptr<QMenu>           _trayIconMenu;
	std::unique_ptr<QMenu>           _trayIconEfxMenu;

	std::unique_ptr<QPixmap> _quitIcon;
	std::unique_ptr<QPixmap> _colorIcon;
	std::unique_ptr<QPixmap> _settingsIcon;
	std::unique_ptr<QPixmap> _clearIcon;
	std::unique_ptr<QPixmap> _effectsIcon;
	std::unique_ptr<QPixmap> _autorunIcon;
	std::unique_ptr<QIcon>   _appIcon;

	std::vector<std::shared_ptr<QAction>>  _effects;

	QColorDialog* _colorDlg;

	HyperHdrDaemon* _hyperhdrd;
	HyperHdrInstance* _hyperhdr;
	HyperHdrIManager* _instanceManager;
	quint16            _webPort;
};
