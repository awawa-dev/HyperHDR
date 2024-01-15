#ifndef PCH_ENABLED
	#include <QColor>
	#include <QSettings>
	#include <list>
#endif

#ifndef _WIN32
	#include <unistd.h>
#endif


#include <QPixmap>
#include <QWindow>
#include <QApplication>
#include <QDesktopServices>
#include <QMenu>
#include <QWidget>
#include <QColorDialog>
#include <QCloseEvent>
#include <QSettings>

#include <HyperhdrConfig.h>

#include <utils/ColorRgb.h>
#include <effectengine/EffectDefinition.h>
#include <webserver/WebServer.h>
#include <utils/Logger.h>

#include "HyperHdrDaemon.h"
#include "systray.h"

SysTray::SysTray(HyperHdrDaemon* hyperhdrDaemon, quint16 webPort)
	: QObject(),
	_quitAction(nullptr),
	_startAction(nullptr),
	_stopAction(nullptr),
	_colorAction(nullptr),
	_settingsAction(nullptr),
	_clearAction(nullptr),
	_autorunAction(nullptr),
	_trayIcon(nullptr),
	_trayIconMenu(nullptr),
	_trayIconEfxMenu(nullptr),
	_colorDlg(nullptr),
	_hyperhdrHandle(),
	_webPort(webPort)
{
	Q_INIT_RESOURCE(resources);

	std::shared_ptr<HyperHdrManager> instanceManager;
	hyperhdrDaemon->getInstanceManager(instanceManager);
	_instanceManager = instanceManager;
	connect(instanceManager.get(), &HyperHdrManager::SignalInstanceStateChanged, this, &SysTray::signalInstanceStateChangedHandler);
	connect(instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, this, &SysTray::signalSettingsChangedHandler);
}

SysTray::~SysTray()
{
	printf("Releasing SysTray\n");

	if (_trayIconEfxMenu != nullptr)
	{
		for (QAction*& effect : _trayIconEfxMenu->actions())
			delete effect;
		_trayIconEfxMenu->clear();
	}

	if (_trayIconMenu != nullptr)
		_trayIconMenu->clear();

	delete _quitAction;
	delete _startAction;
	delete _stopAction;
	delete _colorAction;
	delete _settingsAction;
	delete _clearAction;
	delete _autorunAction;

	delete _trayIcon;
	delete _trayIconEfxMenu;
	delete _trayIconMenu;
	delete _colorDlg;
}

void SysTray::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason)
	{
#ifdef _WIN32
		case QSystemTrayIcon::Context:
			getCurrentAutorunState();
			break;
#endif
		case QSystemTrayIcon::Trigger:
			break;
		case QSystemTrayIcon::DoubleClick:
			settings();
			break;
		case QSystemTrayIcon::MiddleClick:
			break;
		default:;
	}
}

void SysTray::createTrayIcon()
{

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))

#else
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	_trayIconMenu = new QMenu();
	_trayIcon = new QSystemTrayIcon();
	_trayIcon->setContextMenu(_trayIconMenu);

	_quitAction = new QAction(tr("&Quit"));
	_quitAction->setIcon(QPixmap(":/quit.svg"));
	connect(_quitAction, &QAction::triggered, QApplication::instance(), &QApplication::quit);

	_colorAction = new QAction(tr("&Color"));
	_colorAction->setIcon(QPixmap(":/color.svg"));
	connect(_colorAction, &QAction::triggered, this, &SysTray::showColorDialog);

	_settingsAction = new QAction(tr("&Settings"));
	_settingsAction->setIcon(QPixmap(":/settings.svg"));
	connect(_settingsAction, &QAction::triggered, this, &SysTray::settings);

	_clearAction = new QAction(tr("&Clear"));
	_clearAction->setIcon(QPixmap(":/clear.svg"));
	connect(_clearAction, &QAction::triggered, this, &SysTray::clearEfxColor);

	std::list<EffectDefinition> efxs;

	auto _hyperhdr = _hyperhdrHandle.lock();
	if (_hyperhdr)
		SAFE_CALL_0_RET(_hyperhdr.get(), getEffects, std::list<EffectDefinition>, efxs);

	_trayIconEfxMenu = new QMenu();
	_trayIconEfxMenu->setIcon(QPixmap(":/effects.svg"));
	_trayIconEfxMenu->setTitle(tr("Effects"));

	for (const EffectDefinition& efx : efxs)
	{
		QString effectName = efx.name;
		QAction* efxAction = new QAction(effectName);
		connect(efxAction, &QAction::triggered, this, &SysTray::setEffect);
		_trayIconEfxMenu->addAction(efxAction);
	}

#ifdef _WIN32
	_autorunAction = new QAction(tr("&Disable autostart"));
	_autorunAction->setIcon(QPixmap(":/autorun.svg"));
	connect(_autorunAction, &QAction::triggered, this, &SysTray::setAutorunState);

	_trayIconMenu->addAction(_autorunAction);
	_trayIconMenu->addSeparator();
#endif

	_trayIconMenu->addAction(_settingsAction);
	_trayIconMenu->addSeparator();
	_trayIconMenu->addAction(_colorAction);
	_trayIconMenu->addMenu(_trayIconEfxMenu);
	_trayIconMenu->addAction(_clearAction);
	_trayIconMenu->addSeparator();
	_trayIconMenu->addAction(_quitAction);
}

#ifdef _WIN32
bool SysTray::getCurrentAutorunState()
{
	QSettings reg("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	if (reg.value("Hyperhdr", 0).toString() == qApp->applicationFilePath().replace('/', '\\'))
	{
		_autorunAction->setText(tr("&Disable autostart"));
		return true;
	}

	_autorunAction->setText(tr("&Enable autostart"));
	return false;
}
#endif

void SysTray::setAutorunState()
{
#ifdef _WIN32
	bool currentState = getCurrentAutorunState();
	QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	(currentState)
		? reg.remove("Hyperhdr")
		: reg.setValue("Hyperhdr", qApp->applicationFilePath().replace('/', '\\'));
#endif
}

void SysTray::setColor(const QColor& color)
{
	std::vector<ColorRgb> rgbColor{ ColorRgb{ (uint8_t)color.red(), (uint8_t)color.green(), (uint8_t)color.blue() } };

	auto _hyperhdr = _hyperhdrHandle.lock();
	if (_hyperhdr)
		QUEUE_CALL_3(_hyperhdr.get(), setColor, int, 1, std::vector<ColorRgb>, rgbColor, int, 0);
}

void SysTray::showColorDialog()
{
	if (_colorDlg == nullptr)
	{
		_colorDlg = new QColorDialog();
		_colorDlg->setOptions(QColorDialog::NoButtons);
		connect(_colorDlg, SIGNAL(currentColorChanged(const QColor&)), this, SLOT(setColor(const QColor&)));
	}

	if (_colorDlg->isVisible())
	{
		_colorDlg->hide();
	}
	else
	{
		_colorDlg->show();
	}
}

void SysTray::settings()
{
#ifndef _WIN32
	// Hide error messages when opening webbrowser

	int out_pipe[2];
	int saved_stdout;
	int saved_stderr;

	// saving stdout and stderr file descriptor
	saved_stdout = ::dup(STDOUT_FILENO);
	saved_stderr = ::dup(STDERR_FILENO);

	if (::pipe(out_pipe) == 0)
	{
		// redirecting stdout to pipe
		::dup2(out_pipe[1], STDOUT_FILENO);
		::close(out_pipe[1]);
		// redirecting stderr to stdout
		::dup2(STDOUT_FILENO, STDERR_FILENO);
	}
#endif

	QDesktopServices::openUrl(QUrl("http://localhost:" + QString::number(_webPort) + "/", QUrl::TolerantMode));

#ifndef _WIN32
	// restoring stdout
	::dup2(saved_stdout, STDOUT_FILENO);
	// restoring stderr
	::dup2(saved_stderr, STDERR_FILENO);
#endif
}

void SysTray::setEffect()
{
	QString efxName = qobject_cast<QAction*>(sender())->text();
	auto _hyperhdr = _hyperhdrHandle.lock();
	if (_hyperhdr)
		QUEUE_CALL_2(_hyperhdr.get(), setEffect, QString, efxName, int, 1);
}

void SysTray::clearEfxColor()
{
	auto _hyperhdr = _hyperhdrHandle.lock();
	if (_hyperhdr)
		QUEUE_CALL_1(_hyperhdr.get(), clear, int, 1);
}

void SysTray::signalInstanceStateChangedHandler(InstanceState state, quint8 instance, const QString& name)
{
	switch (state) {
		case InstanceState::START:
			if (instance == 0)
			{
				std::shared_ptr<HyperHdrManager> instanceManager = _instanceManager.lock();

				if (instanceManager == nullptr)
					return;

				std::shared_ptr<HyperHdrInstance> retVal;
				SAFE_CALL_1_RET(instanceManager.get(), getHyperHdrInstance, std::shared_ptr<HyperHdrInstance>, retVal, quint8, 0);

				_hyperhdrHandle = retVal;

				createTrayIcon();

				connect(_trayIcon, &QSystemTrayIcon::activated, this, &SysTray::iconActivated);				

				_trayIcon->setIcon(QIcon(":/hyperhdr-icon-32px.png"));
				_trayIcon->show();		
			}

			break;
		default:
			break;
	}
}

void SysTray::signalSettingsChangedHandler(settings::type type, const QJsonDocument& data)
{
	if (type == settings::type::WEBSERVER)
	{
		_webPort = data.object()["port"].toInt();
	}
}
