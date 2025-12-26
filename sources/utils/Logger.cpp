/* Logger.cpp
* 
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#ifndef PCH_ENABLED
	#include <QDateTime>
	#include <QFileInfo>
	#include <QJsonObject>
	
	#include <iostream>
	#include <algorithm>
	#include <ctime>

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

namespace
{	
	#ifdef _WIN32
		HANDLE consoleHandle = nullptr;
	#endif

	constexpr auto MAX_LOGS_NUMBER_SIZE = 500;
	//constexpr auto MAX_REPEAT_MESSAGE_CYCLE = 10;
	constexpr size_t MAX_IDENTIFICATION_LENGTH = 22;

	QString LogLevelToString(Logger::LogLevel lvl) {
		switch (lvl) {
			case Logger::LogLevel::UNSET:   return "";
			case Logger::LogLevel::DEBUG:   return "DEBUG";
			case Logger::LogLevel::INFO:    return "INFO";
			case Logger::LogLevel::WARNING: return "WARNING";
			case Logger::LogLevel::ERRORR:  return "ERROR";
			case Logger::LogLevel::OFF:     return "";
		}
		return "";
	}
}

QString Logger::normalizeFormat(const char* fmt) {
	QString qfmt = QString::fromUtf8(fmt);
	QString result;
	result.reserve(qfmt.size());

	for (int i = 0, argIndex = 1; i < qfmt.size(); ++i) {
		QChar c = qfmt[i];
		if (c == '{') {
			if (i + 1 < qfmt.size() && qfmt[i + 1] == '{') {
				result += '{';
				++i;
			}
			else {
				result += '%' + QString::number(argIndex++);
				while (i < qfmt.size() && qfmt[i] != '}') ++i;
			}
		}
		else if (c == '}' && i + 1 < qfmt.size() && qfmt[i + 1] == '}') {
			result += '}';
			++i;
		}
		else {
			result += c;
		}
	}
	return result;
}


Logger* Logger::getInstance()
{
	static Logger* globalLogger = new Logger();
	return globalLogger;
}

Logger::Logger()
	: QObject()
	, _logLevel(LogLevel::DEBUG)
{
	qRegisterMetaType<Logger::T_LOG_MESSAGE>();

	// detect if we can output colorized logs
	#ifdef _WIN32
		consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		_hasConsole = (consoleHandle != nullptr);
	#else
		_hasConsole = isatty(fileno(stdin));
	#endif


	std::string message = (_hasConsole) ? "TTY is attached to the log output" : "TTY is not attached to the log output";
	storeMessage("LOGGER", LogLevel::INFO, __FILE__, __func__, __LINE__, message);	
}

Logger::~Logger()
{
	if (_hasConsole || _forceVerbose)
	{
#ifndef _WIN32				
		std::cout << "\033[0m";
#endif
	}
}

void Logger::storeMessage(const LoggerName& logName, LogLevel level, const char* sourceFile, const char* func, unsigned int line, const std::string& msg)
{
	if (_logLevel == Logger::UNSET || level < _logLevel)
		return;

	Logger::T_LOG_MESSAGE logMsg;
	QDateTime timeNow = QDateTime::currentDateTime();

	logMsg.loggerName = logName;
	logMsg.function = QString(func);
	logMsg.line = line;
	logMsg.fileName = FileUtils::getBaseName(sourceFile);
	logMsg.utime = timeNow.toMSecsSinceEpoch();
	logMsg.message = QString::fromStdString(msg);
	logMsg.levelString = LogLevelToString(level);

	{
		std::lock_guard logGuard(_logLock);

		if (level == Logger::ERRORR)
			_lastError = QString("%1 [%2] %3").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(logName).arg(logMsg.message);		

		if (_hasConsole || _forceVerbose)
		{
			QString location, prefix, sufix;

			if (level == Logger::DEBUG)
			{
				location = QString("%1:%2:%3() | ")
					.arg(logMsg.fileName)
					.arg(logMsg.line)
					.arg(logMsg.function);
			}

			QString name = logMsg.loggerName;
			name.resize(MAX_IDENTIFICATION_LENGTH, ' ');

		#ifndef _WIN32				
			prefix = "\033[0m";
			sufix = "\033[36;1m";
		#else
			SetConsoleTextAttribute(consoleHandle, 7);
		#endif

			std::cout << QString("%1%2 %3 : ")
				.arg(prefix)
				.arg(timeNow.toString("hh:mm:ss.zzz"))
				.arg(name)
				.toStdString();
			bool isDaemon = (level == Logger::INFO && logMsg.loggerName == "DAEMON" && logMsg.message.indexOf("[") > 0);

		#ifndef _WIN32
			switch (level)
			{
				case(Logger::INFO): prefix = (isDaemon) ? "\033[33;1m" : "\033[32;1m"; break;
				case(Logger::WARNING): prefix = "\033[93;1m"; break;
				case(Logger::ERRORR): prefix = "\033[31;1m"; break;
				default: break;
			}
		#else
			switch (level)
			{
				case(Logger::INFO):SetConsoleTextAttribute(consoleHandle, (isDaemon) ? 6 : 10); break;
				case(Logger::WARNING):SetConsoleTextAttribute(consoleHandle, 14); break;
				case(Logger::ERRORR):SetConsoleTextAttribute(consoleHandle, 12); break;
				default: break;
			}
		#endif
			std::cout << QString("%1<%2> %3%4%5")
				.arg(prefix)
				.arg(logMsg.levelString)
				.arg(location)
				.arg(logMsg.message)
				.arg(sufix)
				.toStdString()
				<< std::endl;
		#ifdef _WIN32		
			SetConsoleTextAttribute(consoleHandle, 11);
		#endif
		}

		_logs.push_back(logMsg);
		if (_logs.size() > MAX_LOGS_NUMBER_SIZE)
		{
			_logs.pop_front();
		}
	}

	emit SignalNewLogMessage(logMsg);
}

QString Logger::getLastError() const
{
	return _lastError;
}

void Logger::forceVerbose()
{
	_forceVerbose = true;
}

void Logger::setLogLevel(Logger::LogLevel level)
{
	_logLevel = level;
}

Logger::LogLevel Logger::getLogLevel() const
{
	return _logLevel;
}

QJsonArray Logger::getLogMessageBuffer()
{
	std::lock_guard logGuard(_logLock);

	QJsonArray messageArray;
	for (const auto& item : _logs)
	{
		QJsonObject message;
		message["loggerName"] = item.loggerName;
		message["function"] = item.function;
		message["line"] = QString::number(item.line);
		message["fileName"] = item.fileName;
		message["message"] = item.message;
		message["levelString"] = item.levelString;
		message["utime"] = QString::number(item.utime);

		messageArray.append(message);
	}
	return messageArray;
}

void Logger::qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
	Logger::LogLevel level;
	switch (type) {
		case QtDebugMsg:	level = Logger::LogLevel::DEBUG; break;
		case QtInfoMsg:		level = Logger::LogLevel::INFO; break;
		case QtWarningMsg:	level = Logger::LogLevel::WARNING; break;
		case QtCriticalMsg:	level = Logger::LogLevel::ERRORR; break;
		case QtFatalMsg:	level = Logger::LogLevel::ERRORR; break;
		default:			level = Logger::LogLevel::INFO; break;
	}
	Logger::getInstance()->storeMessage("LOGGER", level, (context.file) ? context.file : "QT", (context.function) ? context.function : "qDebug", context.line, msg.toUtf8().toStdString());
}



