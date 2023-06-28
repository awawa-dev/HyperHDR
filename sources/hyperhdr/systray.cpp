
#include <list>
#ifndef _WIN32
#include <unistd.h>
#endif

// QT includes
#include <QPixmap>
#include <QWindow>
#include <QGuiApplication>
#include <QWidget>
#include <QColor>
#include <QDesktopServices>
#include <QSettings>

#include <utils/ColorRgb.h>
#include <effectengine/EffectDefinition.h>
#include <webserver/WebServer.h>

#include "hyperhdr.h"
#include "systray.h"

SysTray::SysTray(HyperHdrDaemon* hyperhdrd)
	: QWidget()
	, _colorDlg(nullptr)
	, _hyperhdrd(hyperhdrd)
	, _hyperhdr(nullptr)
	, _instanceManager(HyperHdrIManager::getInstance())
	, _webPort(8090)
{
	Q_INIT_RESOURCE(resources);

	// instance changes
	connect(_instanceManager, &HyperHdrIManager::instanceStateChanged, this, &SysTray::handleInstanceStateChange);
}

SysTray::~SysTray()
{
	if (_trayIconMenu != nullptr)
		_trayIconMenu->clear();
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

	_trayIconMenu = std::unique_ptr<QMenu>(new QMenu(this));
	_trayIcon = std::unique_ptr<QSystemTrayIcon>(new QSystemTrayIcon(this));
	_trayIcon->setContextMenu(_trayIconMenu.get());

	_quitAction = std::unique_ptr<QAction>(new QAction(tr("&Quit"), this));
	_quitIcon = std::unique_ptr<QPixmap>(new QPixmap(":/quit.svg"));
	_quitAction->setIcon(*_quitIcon.get());
	connect(_quitAction.get(), SIGNAL(triggered()), qApp, SLOT(quit()));

	_colorAction = std::unique_ptr<QAction>(new QAction(tr("&Color"), this));
	_colorIcon = std::unique_ptr<QPixmap>(new QPixmap(":/color.svg"));
	_colorAction->setIcon(*_colorIcon.get());
	connect(_colorAction.get(), SIGNAL(triggered()), this, SLOT(showColorDialog()));

	_settingsAction = std::unique_ptr<QAction>(new QAction(tr("&Settings"), this));
	_settingsIcon = std::unique_ptr<QPixmap>(new QPixmap(":/settings.svg"));
	_settingsAction->setIcon(*_settingsIcon.get());
	connect(_settingsAction.get(), SIGNAL(triggered()), this, SLOT(settings()));

	_clearAction = std::unique_ptr<QAction>(new QAction(tr("&Clear"), this));
	_clearIcon = std::unique_ptr<QPixmap>(new QPixmap(":/clear.svg"));
	_clearAction->setIcon(*_clearIcon.get());
	connect(_clearAction.get(), SIGNAL(triggered()), this, SLOT(clearEfxColor()));

	const std::list<EffectDefinition> efxs = _hyperhdr->getEffects();

	_trayIconEfxMenu = std::unique_ptr<QMenu>(new QMenu(_trayIconMenu.get()));
	_effectsIcon = std::unique_ptr<QPixmap>(new QPixmap(":/effects.svg"));
	_trayIconEfxMenu->setIcon(*_effectsIcon.get());
	_trayIconEfxMenu->setTitle(tr("Effects"));

	_effects.clear();
	for (auto efx : efxs)
	{
		std::shared_ptr<QAction> efxAction = std::shared_ptr<QAction>(new QAction(efx.name, this));
		connect(efxAction.get(), SIGNAL(triggered()), this, SLOT(setEffect()));
		_trayIconEfxMenu->addAction(efxAction.get());
		_effects.push_back(efxAction);
	}

#ifdef _WIN32
	_autorunAction = std::unique_ptr<QAction>(new QAction(tr("&Disable autostart"), this));
	_autorunIcon = std::unique_ptr<QPixmap>(new QPixmap(":/autorun.svg"));

	_autorunAction->setIcon(*_autorunIcon.get());
	connect(_autorunAction.get(), SIGNAL(triggered()), this, SLOT(setAutorunState()));

	_trayIconMenu->addAction(_autorunAction.get());
	_trayIconMenu->addSeparator();
#endif

	_trayIconMenu->addAction(_settingsAction.get());
	_trayIconMenu->addSeparator();
	_trayIconMenu->addAction(_colorAction.get());
	_trayIconMenu->addMenu(_trayIconEfxMenu.get());
	_trayIconMenu->addAction(_clearAction.get());
	_trayIconMenu->addSeparator();
	_trayIconMenu->addAction(_quitAction.get());
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

	QMetaObject::invokeMethod(_hyperhdr, "setColor", Qt::QueuedConnection, Q_ARG(int, 1), Q_ARG(std::vector<ColorRgb>, rgbColor), Q_ARG(int, 0));
}

void SysTray::showColorDialog()
{
	if (_colorDlg == nullptr)
	{
		_colorDlg = new QColorDialog(this);
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

void SysTray::closeEvent(QCloseEvent* event)
{
	event->ignore();
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

	if (_hyperhdrd)
	{
		_webPort = _hyperhdrd->getWebPort();
	}

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
	_hyperhdr->setEffect(efxName, 1);
}

void SysTray::clearEfxColor()
{
	_hyperhdr->clear(1);
}

void SysTray::handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name)
{
	switch (state) {
	case InstanceState::H_STARTED:
		if (instance == 0)
		{
			_hyperhdr = _instanceManager->getHyperHdrInstance(0);

			createTrayIcon();

			connect(_trayIcon.get(), SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
				this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
#if !defined(__APPLE__)
			connect(_quitAction.get(), &QAction::triggered, _trayIcon.get(), &QSystemTrayIcon::hide, Qt::DirectConnection);
#endif


			_appIcon = std::unique_ptr<QIcon>(new QIcon(":/hyperhdr-icon-32px.png"));
			_trayIcon->setIcon(*_appIcon.get());
			_trayIcon->show();
#if !defined(__APPLE__)
			setWindowIcon(*_appIcon.get());
#endif
		}

		break;
	default:
		break;
	}
}
