#ifndef PCH_ENABLED
	#include <QDateTime>
	#include <QFileInfo>
	#include <QMutexLocker>
	#include <QJsonObject>
	
	#include <iostream>
	#include <algorithm>
	#include <time.h>

	#include <utils/Logger.h>
#endif

#include <QThreadStorage>
#include <memory>

#include <utils/FileUtils.h>

#ifndef _WIN32
	#include <syslog.h>
	#include <unistd.h>
#elif _WIN32
	#include <io.h>
	#include <windows.h>
	#include <Shlwapi.h>
	#pragma comment(lib, "Shlwapi.lib")
#endif

QMutex  Logger::_mapLock;
QString Logger::_lastError;
bool    Logger::_hasConsole;
QMap<QString, Logger*> Logger::_loggerMap;
QAtomicInteger<int>    Logger::GLOBAL_MIN_LOG_LEVEL{ static_cast<int>(Logger::UNSET) };
QAtomicInteger<bool>   Logger::_forceVerbose = false;

namespace
{
	const char* LogLevelStrings[] = { "", "DEBUG", "INFO", "WARNING", "ERROR" };

	#ifndef _WIN32
		const int    LogLevelSysLog[] = { LOG_DEBUG, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR };
	#else
		HANDLE consoleHandle = nullptr;
	#endif

	const size_t MAX_IDENTIFICATION_LENGTH = 22;

	QAtomicInteger<unsigned int> LoggerCount = 0;
	QAtomicInteger<unsigned int> LoggerId = 0;

	const int MaxRepeatCountSize = 200;
	QThreadStorage<int> RepeatCount;
	QThreadStorage<Logger::T_LOG_MESSAGE> RepeatMessage;
}

Logger* Logger::getInstance(const QString& name, Logger::LogLevel minLevel)
{
	QMutexLocker lock(&_mapLock);

	Logger* log = _loggerMap.value(name, nullptr);
	if (log == nullptr)
	{
		log = new Logger(name, minLevel);
		_loggerMap.insert(name, log);

		if (_loggerMap.size() == 1)
		{
			// detect if we can output colorized logs
			#ifdef _WIN32
				consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
				_hasConsole = (consoleHandle != nullptr);
			#else
				_hasConsole = isatty(fileno(stdin));
			#endif

			T_LOG_MESSAGE t;
			t.utime = QDateTime::currentDateTime().toMSecsSinceEpoch();
			t.loggerName = name;
			t.level = LogLevel::INFO;
			t.levelString = LogLevelStrings[t.level];
			t.message = (_hasConsole) ? "TTY is attached to the log output" : "TTY is not attached to the log output";
			LoggerManager::getInstance()->handleNewLogMessage(t);
		}
	}

	return log;
}

void Logger::deleteInstance(const QString& name)
{
	QMutexLocker lock(&_mapLock);

	if (name.isEmpty())
	{
		for (auto logger : _loggerMap)
			delete logger;

		_loggerMap.clear();
	}
	else
	{
		delete _loggerMap.value(name, nullptr);
		_loggerMap.remove(name);
	}
}

void Logger::setLogLevel(LogLevel level, const QString& name)
{
	if (name.isEmpty())
	{
		GLOBAL_MIN_LOG_LEVEL = static_cast<int>(level);
	}
	else
	{
		Logger* log = Logger::getInstance(name, level);
		log->setMinLevel(level);
	}
}

Logger::LogLevel Logger::getLogLevel(const QString& name)
{
	if (name.isEmpty())
	{
		return static_cast<Logger::LogLevel>(int(GLOBAL_MIN_LOG_LEVEL));
	}

	const Logger* log = Logger::getInstance(name);
	return log->getMinLevel();
}

void Logger::disable()
{
	emit newState(false);
}

void Logger::enable()
{
	emit newState(true);
}

Logger::Logger(const QString& name, LogLevel minLevel)
	: QObject()
	, _name(name)
	, _syslogEnabled(false)
	, _loggerId(LoggerId++)
	, _instanceEnabled(true)
	, _minLevel(static_cast<int>(minLevel))
{
	qRegisterMetaType<Logger::T_LOG_MESSAGE>();

	_logsManager = LoggerManager::getInstance();
	connect(this, &Logger::newLogMessage, _logsManager.get(), &LoggerManager::handleNewLogMessage);
	connect(this, &Logger::newState, _logsManager.get(), &LoggerManager::handleNewState);

	if (LoggerCount.fetchAndAddOrdered(1) == 1)
	{
#ifndef _WIN32
		if (_syslogEnabled)
		{
			openlog("HyperHDR", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
		}
#endif
	}
}

Logger::~Logger()
{
	if (_hasConsole || _forceVerbose)
	{
#ifndef _WIN32				
		std::cout << "\033[0m";
#endif
	}

	if (LoggerCount.fetchAndSubOrdered(1) == 0)
	{
#ifndef _WIN32
		if (_syslogEnabled)
		{
			closelog();
		}
#endif
	}
}

void Logger::write(const Logger::T_LOG_MESSAGE& message)
{
	static QMutex localWriteMutex;

	if (_hasConsole || _forceVerbose)
	{
		QString location, prefix, sufix;

		if (message.level == Logger::DEBUG)
		{
			location = QString("%1:%2:%3() | ")
				.arg(message.fileName)
				.arg(message.line)
				.arg(message.function);
		}

		QString name = message.loggerName;
		name.resize(MAX_IDENTIFICATION_LENGTH, ' ');

		const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(message.utime);

		localWriteMutex.lock();

		#ifndef _WIN32				
			prefix = "\033[0m";
			sufix = "\033[36;1m";
		#else
			SetConsoleTextAttribute(consoleHandle, 7);
		#endif

		std::cout << QString("%1%2 %3 : ")
			.arg(prefix)
			.arg(timestamp.toString("hh:mm:ss.zzz"))
			.arg(name)
			.toStdString();
		bool isDaemon = (message.level == Logger::INFO && message.loggerName == "DAEMON" && message.message.indexOf("[") > 0);

		#ifndef _WIN32
			switch (message.level)
			{
				case(Logger::INFO): prefix = (isDaemon) ? "\033[33;1m" : "\033[32;1m"; break;
				case(Logger::WARNING): prefix = "\033[93;1m"; break;
				case(Logger::ERRORR): prefix = "\033[31;1m"; break;
				default: break;
			}
		#else
			switch (message.level)
			{
				case(Logger::INFO):SetConsoleTextAttribute(consoleHandle, (isDaemon)? 6 : 10); break;
				case(Logger::WARNING):SetConsoleTextAttribute(consoleHandle, 14); break;
				case(Logger::ERRORR):SetConsoleTextAttribute(consoleHandle, 12); break;
				default: break;
			}
		#endif
		std::cout << QString("%1<%2> %3%4%5")
			.arg(prefix)
			.arg(LogLevelStrings[message.level])
			.arg(location)
			.arg(message.message)
			.arg(sufix)
			.toStdString()
			<< std::endl;
		#ifdef _WIN32		
			SetConsoleTextAttribute(consoleHandle, 11);
		#endif
		
		localWriteMutex.unlock();
	}

	newLogMessage(message);
}

void Logger::setInstanceEnable(bool enabled)
{
	_instanceEnabled = enabled;
}

void Logger::Message(LogLevel level, const char* sourceFile, const char* func, unsigned int line, const char* fmt, ...)
{
	if (!_instanceEnabled)
		return;

	Logger::LogLevel globalLevel = static_cast<Logger::LogLevel>(int(GLOBAL_MIN_LOG_LEVEL));
	bool writeAnyway = false;
	bool repeatMessage = false;

	if ((globalLevel == Logger::UNSET && level < _minLevel) // no global level, use level from logger
		|| (globalLevel > Logger::UNSET && level < globalLevel)) // global level set, use global level
		return;

	const size_t max_msg_length = 1024;
	char msg[max_msg_length];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, max_msg_length, fmt, args);
	va_end(args);

	const auto repeatedSummary = [&]
	{
		if (RepeatCount.localData() > 10)
		{
			Logger::T_LOG_MESSAGE repMsg = RepeatMessage.localData();
			repMsg.message = "Previous line repeats " + QString::number(RepeatCount.localData() - 10) + " times";
			repMsg.utime = QDateTime::currentMSecsSinceEpoch();

			write(repMsg);
#ifndef _WIN32
			if (_syslogEnabled && repMsg.level >= Logger::WARNING)
				syslog(LogLevelSysLog[repMsg.level], "Previous line repeats %d times", RepeatCount.localData() - 10);
#endif
		}
		RepeatCount.setLocalData(0);
	};

	if (RepeatMessage.localData().loggerName == _name &&
		RepeatMessage.localData().function == func &&
		RepeatMessage.localData().message == msg &&
		RepeatMessage.localData().line == line)
	{
		repeatMessage = true;
		if (RepeatCount.localData() >= MaxRepeatCountSize)
			repeatedSummary();
		else
			RepeatCount.setLocalData(RepeatCount.localData() + 1);

		if (RepeatCount.localData() < 10)
			writeAnyway = true;
	}

	if (!repeatMessage || writeAnyway)
	{
		if (!repeatMessage && RepeatCount.localData())
			repeatedSummary();

		Logger::T_LOG_MESSAGE logMsg;

		logMsg.loggerName = _name;
		logMsg.function = QString(func);
		logMsg.line = line;
		logMsg.fileName = FileUtils::getBaseName(sourceFile);
		logMsg.utime = QDateTime::currentMSecsSinceEpoch();
		logMsg.message = QString(msg);
		logMsg.level = level;
		logMsg.levelString = LogLevelStrings[level];

		write(logMsg);
#ifndef _WIN32
		if (_syslogEnabled && level >= Logger::WARNING)
			syslog(LogLevelSysLog[level], "%s", msg);
#endif
		if (level == Logger::ERRORR)
			_lastError = QString("%1 [%2] %3").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(_name).arg(QString(msg));

		RepeatMessage.setLocalData(logMsg);
	}
}

QString Logger::getLastError()
{
	return _lastError;
}

void Logger::forceVerbose()
{
	_forceVerbose = true;
}

void Logger::setMinLevel(Logger::LogLevel level)
{
	_minLevel = static_cast<int>(level);
}

Logger::LogLevel Logger::getMinLevel() const
{
	return static_cast<LogLevel>(int(_minLevel));
}

QString Logger::getName() const
{
	return _name;
}

LoggerManager::LoggerManager()
	: QObject()
	, _loggerMaxMsgBufferSize(500)
	, _enable(true)
{
	_logs.reserve(_loggerMaxMsgBufferSize);
}

LoggerManager::~LoggerManager()
{
	std::cout << "Clean-up logs..." << std::endl;

	while (_logs.length() > 0)
	{
		delete (_logs.takeFirst());
	}

	std::cout << "Goodbye!" << std::endl;
}

QJsonArray LoggerManager::getLogMessageBuffer()
{
	QJsonArray messageArray;
	for (auto item : _logs)
	{
		QJsonObject message;
		message["loggerName"] = item->loggerName;
		message["function"] = item->function;
		message["line"] = QString::number(item->line);
		message["fileName"] = item->fileName;
		message["message"] = item->message;
		message["levelString"] = item->levelString;
		message["utime"] = QString::number(item->utime);

		messageArray.append(message);
	}
	return messageArray;
}

void LoggerManager::handleNewLogMessage(const Logger::T_LOG_MESSAGE& _msg)
{
	if (!_enable && _msg.level != Logger::LogLevel::ERRORR)
		return;

	Logger::T_LOG_MESSAGE* msg = new Logger::T_LOG_MESSAGE(_msg);

	_logs.push_back(msg);
	if (_logs.length() > _loggerMaxMsgBufferSize)
	{
		delete (_logs.takeFirst());
	}

	emit newLogMessage(_msg);
}

std::shared_ptr<LoggerManager> LoggerManager::getInstance()
{
	static QMutex locker;
	static std::weak_ptr<LoggerManager> persist;
	QMutexLocker lockThis(&locker);

	auto result = persist.lock();
	if (!result)
	{
		result = std::shared_ptr<LoggerManager>(
			new LoggerManager(),
			[](LoggerManager* oldManager) {
				hyperhdr::SMARTPOINTER_MESSAGE("LoggerManager");
				delete oldManager;
			}
		);
		persist = result;
	}
	return result;
}

void LoggerManager::handleNewState(bool state)
{
	_enable = state;
}

