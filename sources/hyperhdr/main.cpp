#ifndef PCH_ENABLED
	#include <QCoreApplication>
	#include <QLocale>
	#include <QFile>
	#include <QString>
	#include <QResource>
	#include <QDir>
	#include <QStringList>
	#include <QStringList>

	#include <exception>
	#include <iostream>
	#include <cassert>
	#include <stdlib.h>
	#include <stdio.h>
#endif

#include <csignal>

#if !defined(__APPLE__) && !defined(_WIN32)
	#include <sys/prctl.h>
#endif


#ifdef _WIN32
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
	#include <process.h>
#else
	#include <unistd.h>
#endif

#include <HyperhdrConfig.h>

#include "SystrayHandler.h"
#include <utils/Logger.h>
#include <commandline/Parser.h>
#include <utils/DefaultSignalHandler.h>
#include <db/AuthTable.h>

#include "detectProcess.h"
#include "HyperHdrDaemon.h"

using namespace commandline;

#if defined(WIN32)

void CreateConsole()
{
	if (!AllocConsole()) {
		return;
	}

	FILE* fDummy;
	freopen_s(&fDummy, "CONOUT$", "w", stdout);
	freopen_s(&fDummy, "CONOUT$", "w", stderr);
	freopen_s(&fDummy, "CONIN$", "r", stdin);
	SetConsoleTitle(TEXT("HyperHDR"));
}

#endif

#define PERM0664 QFileDevice::ReadOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteOwner | QFileDevice::WriteGroup

HyperHdrDaemon* hyperhdrd = nullptr;

QCoreApplication* createApplication(bool& isGuiApp, int& argc, char* argv[])
{
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
	isGuiApp = true && !forceNoGui;
#else
	if (!forceNoGui)
	{
		isGuiApp = (getenv("DISPLAY") != NULL &&
			((getenv("XDG_SESSION_TYPE") != NULL && strcmp(getenv("XDG_SESSION_TYPE"), "tty") != 0) || getenv("WAYLAND_DISPLAY") != NULL));
		std::cout << ((isGuiApp) ? "GUI" : "Console") << " application: " << std::endl;
	}
#endif

	QCoreApplication* app = new QCoreApplication(argc, argv);
	app->setApplicationName("HyperHdr");
	app->setApplicationVersion(HYPERHDR_VERSION);
	// add optional library path
	app->addLibraryPath(QCoreApplication::applicationDirPath() + "/../lib");

	return app;
}

int main(int argc, char** argv)
{
	QStringList params;

	// check if we are running already an instance
	// TODO Allow one session per user
#ifdef _WIN32
	const char* processName = "hyperhdr.exe";
#else
	const char* processName = "hyperhdr";
#endif

	// Initialising QCoreApplication
	bool isGuiApp = false;
	QScopedPointer<QCoreApplication> app(createApplication(isGuiApp, argc, argv));

	DefaultSignalHandler::install();

	// force the locale
	setlocale(LC_ALL, "C");
	QLocale::setDefault(QLocale::c());

	Parser parser("HyperHDR Daemon");
	parser.addHelpOption();

	BooleanOption& versionOption = parser.add<BooleanOption>(0x0, "version", "Show version information");
	Option& userDataOption = parser.add<Option>('u', "userdata", "Overwrite user data path, defaults to home directory of current user (%1)", QDir::homePath() + "/.hyperhdr");
	BooleanOption& resetPassword = parser.add<BooleanOption>(0x0, "resetPassword", "Lost your password? Reset it with this option back to 'hyperhdr'");
	BooleanOption& deleteDB = parser.add<BooleanOption>(0x0, "deleteDatabase", "Start all over? This Option will delete the database");
	BooleanOption& silentOption = parser.add<BooleanOption>('s', "silent", "Do not print any outputs");
	BooleanOption& verboseOption = parser.add<BooleanOption>('v', "verbose", "Increase verbosity");
	BooleanOption& debugOption = parser.add<BooleanOption>('d', "debug", "Show debug messages");
#ifdef ENABLE_PIPEWIRE
	BooleanOption& pipewireOption = parser.add<BooleanOption>(0x0, "pipewire", "Force pipewire screen grabber if it's available");
#endif
#ifdef WIN32
	BooleanOption& consoleOption = parser.add<BooleanOption>('c', "console", "Open a console window to view log output");
#endif
	parser.add<BooleanOption>(0x0, "desktop", "Show systray on desktop");
	parser.add<BooleanOption>(0x0, "service", "Force HyperHdr to start as console service");

	/* Internal options, invisible to help */
	BooleanOption& waitOption = parser.addHidden<BooleanOption>(0x0, "wait-hyperhdr", "Do not exit if other HyperHdr instances are running, wait them to finish");

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
			std::cerr << "The HyperHDR Daemon is already running, abort start";
			return 1;
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

	// initialize main logger and set global log level
	Logger* log = Logger::getInstance("MAIN");
	Logger::setLogLevel(Logger::INFO);

	int logLevelCheck = 0;
	if (parser.isSet(silentOption))
	{
		Logger::setLogLevel(Logger::OFF);
		logLevelCheck++;
	}

	if (parser.isSet(verboseOption))
	{
		Logger::forceVerbose();
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

#ifdef ENABLE_PIPEWIRE
	if (parser.isSet(pipewireOption))
	{
		params.append("pipewire");
	}
#endif

	int rc = 1;
	bool readonlyMode = false;

	QString userDataPath(userDataOption.value(parser));

	QDir userDataDirectory(userDataPath);

	QFileInfo dbFile(userDataDirectory.absolutePath() + "/db/hyperhdr.db");

	try
	{
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

				Info(log, "Database path: '%s', readonlyMode = %s", QSTRING_CSTR(dbFile.absoluteFilePath()), (readonlyMode) ? "enabled" : "disabled");
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

		DBManager::initializeDatabaseFilename(dbFile, readonlyMode);

		// reset Password without spawning daemon
		if (parser.isSet(resetPassword))
		{
			if (readonlyMode)
			{
				Error(log, "Password reset is not possible. The user data path '%s' is not writeable.", QSTRING_CSTR(userDataDirectory.absolutePath()));
				throw std::runtime_error("Password reset failed");
			}
			else
			{
				std::unique_ptr<AuthTable> table = std::unique_ptr<AuthTable>(new AuthTable());
				if (table->resetHyperhdrUser()) {
					Info(log, "Password reset successful");
					exit(0);
				}
				else {
					Error(log, "Failed to reset password!");
					exit(1);
				}
			}
		}

		// delete database before start
		if (parser.isSet(deleteDB))
		{
			if (readonlyMode)
			{
				Error(log, "Deleting the configuration database is not possible. The user data path '%s' is not writeable.", QSTRING_CSTR(dbFile.absolutePath()));
				throw std::runtime_error("Deleting the configuration database failed");
			}
			else
			{
				if (QFile::exists(dbFile.absoluteFilePath()))
				{
					if (!QFile::remove(dbFile.absoluteFilePath()))
					{
						Info(log, "Failed to delete Database!");
						exit(1);
					}
					else
					{
						Info(log, "Configuration database deleted successfully.");
					}
				}
				else
				{
					Warning(log, "Configuration database [%s] does not exist!", QSTRING_CSTR(dbFile.absoluteFilePath()));
				}
			}
		}

		Info(log, "Starting HyperHdr - %s, %s, built: %s:%s", HYPERHDR_VERSION, HYPERHDR_BUILD_ID, __DATE__, __TIME__);
		Debug(log, "QtVersion [%s]", QT_VERSION_STR);

		if (!readonlyMode)
		{
			Info(log, "Set user data path to '%s'", QSTRING_CSTR(userDataDirectory.absolutePath()));
		}
		else
		{
			Warning(log, "The user data path '%s' is not writeable. HyperHdr starts in read-only mode. Configuration updates will not be persisted!", QSTRING_CSTR(userDataDirectory.absolutePath()));
		}

		#ifdef _WIN32
			SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		#endif	

		try
		{
			hyperhdrd = new HyperHdrDaemon(userDataDirectory.absolutePath(), qApp, bool(logLevelCheck), readonlyMode, params, isGuiApp);
		}
		catch (std::exception& e)
		{
			Error(log, "Main HyperHDR service aborted: %s", e.what());
			throw;
		}

		// run the application
		SystrayHandler* systray = (isGuiApp) ? new SystrayHandler(hyperhdrd, hyperhdrd->getWebPort(), userDataDirectory.absolutePath()) : nullptr;

		if (systray != nullptr && systray->isInitialized())
		{
			#ifdef __APPLE__
				QTimer* timer = new QTimer(systray);
				QObject::connect(timer, &QTimer::timeout, systray, [&systray]() {
					systray->loop();
					});

				timer->setInterval(200);			
				timer->start();
			#endif

			rc = app->exec();

			systray->close();
		}
		else
		{
			#if defined(ENABLE_SYSTRAY)
				if (isGuiApp)
				{
					Error(log, "Could not inilized the systray. Make sure that the libgtk-3-0/gtk3 package is installed.");
				}
			#endif
			rc = app->exec();
		}

		Info(log, "The application closed with code %d", rc);

		delete systray;
		systray = nullptr;

		delete hyperhdrd;
		hyperhdrd = nullptr;
	}
	catch (std::exception& e)
	{
		Error(log, "HyperHDR aborted: %s", e.what());
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
