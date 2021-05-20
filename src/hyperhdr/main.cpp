#include <cassert>
#include <csignal>
#include <stdlib.h>
#include <stdio.h>

#if !defined(__APPLE__) && !defined(_WIN32)
/* prctl is Linux only */
#include <sys/prctl.h>
#endif
// getpid()
#ifdef _WIN32
#include "console.h"
#include <process.h>
#else
#include <unistd.h>
#endif

#include <exception>

#include <QCoreApplication>
#include <QApplication>
#include <QLocale>
#include <QFile>
#include <QString>
#include <QResource>
#include <QDir>
#include <QStringList>
#include <QSystemTrayIcon>

#include "HyperhdrConfig.h"

#include <utils/Logger.h>
#include <utils/FileUtils.h>
#include <commandline/Parser.h>
#include <commandline/IntOption.h>
#include <utils/DefaultSignalHandler.h>
#include <../../include/db/AuthTable.h>

#include "detectProcess.h"

#include "hyperhdr.h"
#include "systray.h"

using namespace commandline;

#define PERM0664 QFileDevice::ReadOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteOwner | QFileDevice::WriteGroup

#ifndef _WIN32
void signal_handler(int signum)
{
	// HyperHDR Managment instance
	HyperHdrIManager *_hyperhdr = HyperHdrIManager::getInstance();

	if (signum == SIGCHLD)
	{
		// only quit when a registered child process is gone
		// currently this feature is not active ...
		return;
	}
	else if (signum == SIGUSR1)
	{
		if (_hyperhdr != nullptr)
		{
			_hyperhdr->toggleStateAllInstances(false);
		}
		return;
	}
	else if (signum == SIGUSR2)
	{
		if (_hyperhdr != nullptr)
		{
			_hyperhdr->toggleStateAllInstances(true);
		}
		return;
	}
}
#endif

QCoreApplication* createApplication(int &argc, char *argv[])
{
	bool isGuiApp = false;
	bool forceNoGui = false;
	// command line
	for (int i = 1; i < argc; ++i)
	{
		if (qstrcmp(argv[i], "--desktop") == 0)
		{
			isGuiApp = true;
		}
		else if (qstrcmp(argv[i], "--service") == 0)
		{
			isGuiApp = false;
			forceNoGui = true;
		}
	}

	// on osx/windows gui always available
#if defined(__APPLE__) || defined(_WIN32)
	isGuiApp = true && ! forceNoGui;
#else
	if (!forceNoGui)
	{
		isGuiApp = (getenv("DISPLAY") != NULL && (getenv("XDG_SESSION_TYPE") != NULL || getenv("WAYLAND_DISPLAY") != NULL));
		std::cout << "GUI application";
	}
#endif

	if (isGuiApp)
	{
		QApplication* app = new QApplication(argc, argv);
		// add optional library path
		app->addLibraryPath(QApplication::applicationDirPath() + "/../lib");
		app->setApplicationDisplayName("HyperHdr");
#if defined(__APPLE__)
		app->setWindowIcon(QIcon(":/hyperhdr-icon-64px.png"));
#else
		app->setWindowIcon(QIcon(":/hyperhdr-icon-32px.png"));
#endif
		return app;
	}

	QCoreApplication* app = new QCoreApplication(argc, argv);
	app->setApplicationName("HyperHdr");
	app->setApplicationVersion(HYPERHDR_VERSION);
	// add optional library path
	app->addLibraryPath(QApplication::applicationDirPath() + "/../lib");

	return app;
}

int main(int argc, char** argv)
{
#ifndef _WIN32
	setenv("AVAHI_COMPAT_NOWARN", "1", 1);
#endif
	// initialize main logger and set global log level
	Logger *log = Logger::getInstance("MAIN");
	Logger::setLogLevel(Logger::INFO);

	// check if we are running already an instance
	// TODO Allow one session per user
	#ifdef _WIN32
		const char* processName = "hyperhdr.exe";
	#else
		const char* processName = "hyperhdr";
	#endif

	// Initialising QCoreApplication
	QScopedPointer<QCoreApplication> app(createApplication(argc, argv));

	bool isGuiApp = (qobject_cast<QApplication *>(app.data()) != 0 && QSystemTrayIcon::isSystemTrayAvailable());

	DefaultSignalHandler::install();

#ifndef _WIN32
	signal(SIGCHLD, signal_handler);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);
#endif
	// force the locale
	setlocale(LC_ALL, "C");
	QLocale::setDefault(QLocale::c());

	Parser parser("HyperHDR Daemon");
	parser.addHelpOption();

	BooleanOption & versionOption       = parser.add<BooleanOption> (0x0, "version", "Show version information");
	Option        & userDataOption      = parser.add<Option>        ('u', "userdata", "Overwrite user data path, defaults to home directory of current user (%1)", QDir::homePath() + "/.hyperhdr");
	BooleanOption & resetPassword       = parser.add<BooleanOption> (0x0, "resetPassword", "Lost your password? Reset it with this option back to 'hyperhdr'");
	BooleanOption & deleteDB            = parser.add<BooleanOption> (0x0, "deleteDatabase", "Start all over? This Option will delete the database");
	BooleanOption & silentOption        = parser.add<BooleanOption> ('s', "silent", "Do not print any outputs");
	BooleanOption & verboseOption       = parser.add<BooleanOption> ('v', "verbose", "Increase verbosity");
	BooleanOption & debugOption         = parser.add<BooleanOption> ('d', "debug", "Show debug messages");
#ifdef WIN32
	BooleanOption & consoleOption       = parser.add<BooleanOption> ('c', "console", "Open a console window to view log output");
#endif
	                                      parser.add<BooleanOption> (0x0, "desktop", "Show systray on desktop");
	                                      parser.add<BooleanOption> (0x0, "service", "Force HyperHdr to start as console service");

	/* Internal options, invisible to help */
	BooleanOption & waitOption          = parser.addHidden<BooleanOption> (0x0, "wait-hyperhdr", "Do not exit if other HyperHdr instances are running, wait them to finish");

	parser.process(*qApp);

	if (parser.isSet(versionOption))
	{
		std::cout
			<< "HyperHdr Ambient Light Deamon" << std::endl
			<< "\tVersion   : " << HYPERHDR_VERSION << " (" << HYPERHDR_BUILD_ID << ")" << std::endl
			<< "\tBuild Time: " << __DATE__ << " " << __TIME__ << std::endl;

		return 0;
	}

	if (!parser.isSet(waitOption))
	{
		if (getProcessIdsByProcessName(processName).size() > 1)
		{
			Error(log, "The HyperHdr Daemon is already running, abort start");
			return 0;
		}
	}
	else
	{
		while (getProcessIdsByProcessName(processName).size() > 1)
		{
			QThread::msleep(100);
		}
	}

#ifdef WIN32
	if (parser.isSet(consoleOption))
	{
		CreateConsole();
	}
#endif

	int logLevelCheck = 0;
	if (parser.isSet(silentOption))
	{
		Logger::setLogLevel(Logger::OFF);
		logLevelCheck++;
	}

	if (parser.isSet(verboseOption))
	{
		Logger::setLogLevel(Logger::INFO);
		logLevelCheck++;
	}

	if (parser.isSet(debugOption))
	{
		Logger::setLogLevel(Logger::DEBUG);
		logLevelCheck++;
	}

	if (logLevelCheck > 1)
	{
		Error(log, "aborting, because options --silent --verbose --debug can't be used together");
		return 0;
	}

	int rc = 1;
	bool readonlyMode = false;

	QString userDataPath(userDataOption.value(parser));

	QDir userDataDirectory(userDataPath);

	QFileInfo dbFile(userDataDirectory.absolutePath() + "/db/hyperhdr.db");

	try
	{
		if (!dbFile.exists())
		{
			QDir	  oldUserDataDirectory(QDir::homePath() + "/.hyperion");
			QFileInfo dbOldFile(oldUserDataDirectory.absolutePath() + "/db/hyperion.db");

			if (dbOldFile.exists())
			{
				bool migrate = false;
				if (userDataDirectory.mkpath(dbFile.absolutePath()))
				{
					if (QFile::copy(dbOldFile.absoluteFilePath(), dbFile.absoluteFilePath()))
					{						
						Warning(log, "Migration to the new config: '%s' from '%s'", QSTRING_CSTR(dbFile.absoluteFilePath()), QSTRING_CSTR(dbOldFile.absoluteFilePath()));
						migrate = true;
					}
				}

				if (!migrate)
				{					
					Error(log, "Could not migration to the new config. Please check access/write right for HyperHdr deamon: '%s' from '%s'", QSTRING_CSTR(dbFile.absoluteFilePath()), QSTRING_CSTR(dbOldFile.absoluteFilePath()));
					dbFile = dbOldFile;
					userDataDirectory = oldUserDataDirectory;
				}
			}
		}

		if (dbFile.exists())
		{
			if (!dbFile.isReadable())
			{
				throw std::runtime_error("Configuration database '" + dbFile.absoluteFilePath().toStdString() + "' is not readable. Please setup permissions correctly!");
			}
			else
			{
				if (!dbFile.isWritable())
				{
					readonlyMode = true;
				}

				Info(log, "Database path: '%s', readonlyMode = %s", QSTRING_CSTR(dbFile.absoluteFilePath()), (readonlyMode)? "enabled" : "disabled");
			}
		}
		else
		{
			if (!userDataDirectory.mkpath(dbFile.absolutePath()))
			{
				if (!userDataDirectory.isReadable() || !dbFile.isWritable())
				{
					throw std::runtime_error("The user data path '" + userDataDirectory.absolutePath().toStdString() + "' can't be created or isn't read/writeable. Please setup permissions correctly!");
				}
			}
		}

		// reset Password without spawning daemon
		if(parser.isSet(resetPassword))
		{
			if ( readonlyMode )
			{
				Error(log,"Password reset is not possible. The user data path '%s' is not writeable.", QSTRING_CSTR(userDataDirectory.absolutePath()));
				throw std::runtime_error("Password reset failed");
			}
			else
			{
				AuthTable* table = new AuthTable(userDataDirectory.absolutePath());
				if(table->resetHyperhdrUser()){
					Info(log,"Password reset successful");
					delete table;
					exit(0);
				} else {
					Error(log,"Failed to reset password!");
					delete table;
					exit(1);
				}
			}
		}

		// delete database before start
		if(parser.isSet(deleteDB))
		{
			if ( readonlyMode )
			{
				Error(log,"Deleting the configuration database is not possible. The user data path '%s' is not writeable.", QSTRING_CSTR(dbFile.absolutePath()));
				throw std::runtime_error("Deleting the configuration database failed");
			}
			else
			{
				if (QFile::exists(dbFile.absoluteFilePath()))
				{
					if (!QFile::remove(dbFile.absoluteFilePath()))
					{
						Info(log,"Failed to delete Database!");
						exit(1);
					}
					else
					{
						Info(log,"Configuration database deleted successfully.");
					}
				}
				else
				{
					Warning(log,"Configuration database [%s] does not exist!", QSTRING_CSTR(dbFile.absoluteFilePath()));
				}
			}
		}

		Info(log,"Starting HyperHdr - %s, %s, built: %s:%s", HYPERHDR_VERSION, HYPERHDR_BUILD_ID, __DATE__, __TIME__);
		Debug(log,"QtVersion [%s]", QT_VERSION_STR);

		if ( !readonlyMode )
		{
			Info(log, "Set user data path to '%s'", QSTRING_CSTR(userDataDirectory.absolutePath()));
		}
		else
		{
			Warning(log,"The user data path '%s' is not writeable. HyperHdr starts in read-only mode. Configuration updates will not be persisted!", QSTRING_CSTR(userDataDirectory.absolutePath()));
		}

		HyperHdrDaemon* hyperhdrd = nullptr;
		try
		{
			hyperhdrd = new HyperHdrDaemon(userDataDirectory.absolutePath(), qApp, bool(logLevelCheck), readonlyMode);
		}
		catch (std::exception& e)
		{
			Error(log, "HyperHdr Daemon aborted: %s", e.what());
			throw;
		}

		// run the application
		if (isGuiApp)
		{
			Info(log, "start systray");
			QApplication::setQuitOnLastWindowClosed(false);
			SysTray tray(hyperhdrd);
			tray.hide();
			rc = (qobject_cast<QApplication *>(app.data()))->exec();
		}
		else
		{
			rc = app->exec();
		}
		Info(log, "Application closed with code %d", rc);
		delete hyperhdrd;
	}
	catch (std::exception& e)
	{
		Error(log, "HyperHdr aborted: %s", e.what());
	}

	// delete components
	Logger::deleteInstance();

#ifdef _WIN32
	if (parser.isSet(consoleOption))
	{
		system("pause");
	}
#endif

	return rc;
}
