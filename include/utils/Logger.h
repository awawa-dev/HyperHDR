#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <QMap>
	#include <QAtomicInteger>
	#include <QList>
	#include <QMutex>
	#include <QJsonArray>

	#include <stdio.h>
	#include <stdarg.h>
#endif

#include <utils/InternalClock.h>
#include <utils/Macros.h>

#define THREAD_ID QSTRING_CSTR(QString().asprintf("%p", QThread::currentThreadId()))

#define LOG_MESSAGE(severity, logger, ...)   (logger)->Message(severity, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define REPORT_TOKEN        "<Report>"
#define Debug(logger, ...)   LOG_MESSAGE(Logger::DEBUG  , logger, __VA_ARGS__)
#define Info(logger, ...)    LOG_MESSAGE(Logger::INFO   , logger, __VA_ARGS__)
#define Warning(logger, ...) LOG_MESSAGE(Logger::WARNING, logger, __VA_ARGS__)
#define Error(logger, ...)   LOG_MESSAGE(Logger::ERRORR , logger, __VA_ARGS__)

#define DebugIf(condition, logger, ...)   if (condition) Debug(logger,   __VA_ARGS__)
#define InfoIf(condition, logger, ...)    if (condition) Info(logger,    __VA_ARGS__)
#define WarningIf(condition, logger, ...) if (condition) Warning(logger, __VA_ARGS__)
#define ErrorIf(condition, logger, ...)   if (condition) Error(logger,   __VA_ARGS__)

class LoggerManager;

class Logger : public QObject
{
	Q_OBJECT

public:
	enum LogLevel {
		UNSET = 0,
		DEBUG = 1,
		INFO = 2,
		WARNING = 3,
		ERRORR = 4,
		OFF = 5
	};

	struct T_LOG_MESSAGE
	{
		QString      loggerName;
		QString      function;
		unsigned int line;
		QString      fileName;
		uint64_t     utime;
		QString      message;
		LogLevel     level;
		QString      levelString;

		T_LOG_MESSAGE()
		{
			line = 0;
			utime = 0;
			level = LogLevel::INFO;
		}

		T_LOG_MESSAGE(const T_LOG_MESSAGE&) = default;
	};

	static Logger* getInstance(const QString& name = "", LogLevel minLevel = Logger::INFO);
	static void     deleteInstance(const QString& name = "");
	static void     setLogLevel(LogLevel level, const QString& name = "");
	static void     forceVerbose();
	static LogLevel getLogLevel(const QString& name = "");
	static QString getLastError();

	void     Message(LogLevel level, const char* sourceFile, const char* func, unsigned int line, const char* fmt, ...);
	void     setMinLevel(LogLevel level);
	LogLevel getMinLevel() const;
	QString  getName() const;
	void     disable();
	void     enable();
	void	 setInstanceEnable(bool enabled);

signals:
	void newLogMessage(Logger::T_LOG_MESSAGE);
	void newState(bool state);

protected:
	Logger(const QString& name = "", LogLevel minLevel = INFO);
	~Logger() override;

private:
	void write(const Logger::T_LOG_MESSAGE& message);

	static QMutex                 _mapLock;
	static QMap<QString, Logger*> _loggerMap;
	static QAtomicInteger<int>    GLOBAL_MIN_LOG_LEVEL;
	static QAtomicInteger<bool>   _forceVerbose;
	static QString                _lastError;
	static bool                   _hasConsole;
	const QString                _name;
	const bool                   _syslogEnabled;
	const unsigned               _loggerId;
	bool						 _instanceEnabled;

	std::shared_ptr<LoggerManager> _logsManager;

	QAtomicInteger<int> _minLevel;
};

class LoggerManager : public QObject
{
	Q_OBJECT

public:
	LoggerManager();
	~LoggerManager();

	static std::shared_ptr<LoggerManager> getInstance();

public slots:
	void handleNewLogMessage(const Logger::T_LOG_MESSAGE&);
	void handleNewState(bool state);
	QJsonArray getLogMessageBuffer();

signals:
	void newLogMessage(const Logger::T_LOG_MESSAGE&);

protected:

	QList<Logger::T_LOG_MESSAGE*>	_logs;
	const int						_loggerMaxMsgBufferSize;
	bool							_enable;
};

Q_DECLARE_METATYPE(Logger::T_LOG_MESSAGE)
