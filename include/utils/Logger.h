#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <QMap>
	#include <QAtomicInteger>
	#include <QList>	
	#include <QJsonArray>
	#include <QByteArray>

	#include <stdio.h>
	#include <stdarg.h>
	#include <cstdio>
	#include <type_traits>
	#include <vector>
	#include <list>
	#include <string>
	#include <mutex>
	#include <array>
		
	#if __has_include(<format>)
		#include <format>
	#else
		#pragma message("********** Warning: <format> header not found – std::format unavailable. Fallback to basic logging **********")
	#endif
#endif

#include <utils/InternalClock.h>
#include <utils/Macros.h>

#define REPORT_TOKEN        "<Report>"

class LoggerManager;

typedef QString LoggerName;

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
		QString      levelString;

		T_LOG_MESSAGE()
		{
			line = 0;
			utime = 0;
		}

		T_LOG_MESSAGE(const T_LOG_MESSAGE&) = default;
	};

	static QString normalizeFormat(const char* fmt);
	static Logger* getInstance();
	static void	qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

	void forceVerbose();
	QString getLastError() const;
	
	void     setLogLevel(LogLevel level);
	LogLevel getLogLevel() const;
	void	 storeMessage(const LoggerName& name, LogLevel level, const char* sourceFile, const char* func, unsigned int line, const std::string& msg);

signals:
	void SignalNewLogMessage(Logger::T_LOG_MESSAGE);

public slots:
	QJsonArray getLogMessageBuffer();

protected:
	Logger();
	~Logger() override;

private:
	static inline std::mutex _logLock;
	bool			_forceVerbose;
	QString			_lastError;
	bool			_hasConsole;
	LogLevel		_logLevel;

	std::list<Logger::T_LOG_MESSAGE> _logs;	
};

#if defined(__cpp_lib_format) && ( !defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) || (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 130300) )
	template<typename T>
	auto formatArgConverter(T&& arg) {
		using DecayedT = std::decay_t<T>;
		if constexpr (std::is_same_v<DecayedT, QString>)
		{
			return arg.toStdString();
		}
		else if constexpr (std::is_same_v<DecayedT, QByteArray>)
		{
			return arg.toStdString();
		}
		else
		{
			return std::forward<T>(arg);
		}
	}

	template<typename T, typename D = std::decay_t<T>>
	using LoggedFormatType = std::conditional_t<
		std::is_same_v<D, QString> || std::is_same_v<D, QByteArray>,
		std::string, D>;

	template<typename... Args>
	void formatLogImplementation(const LoggerName& loggerContext, Logger::LogLevel level,
				  const char* fileName,
				  const char* functionName,
				  unsigned int line,
				  std::format_string<LoggedFormatType<Args>...> fmt,
				  Args&&... args) 
	{
		std::string result = std::format(fmt, formatArgConverter(std::forward<Args>(args))...);
		Logger::getInstance()->storeMessage(loggerContext, level, fileName, functionName, line, result);
	}
#else
	template<typename T>
	auto toQStringArgFormat(T&& v) {
		using D = std::decay_t<T>;
		if constexpr (std::is_same_v<D, QString>) {
			return std::forward<T>(v);
		}
		else if constexpr (std::is_same_v<D, QByteArray>) {
			return QString::fromUtf8(v);
		}
		else if constexpr (std::is_same_v<D, std::string>) {
			return QString::fromStdString(v);
		}
		else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>) {
			return QString::fromUtf8(v);
		}
		else if constexpr (std::is_same_v<D, QChar>) {
			return std::forward<T>(v);
		}
		else if constexpr (std::is_arithmetic_v<D>) {
			return std::forward<T>(v);
		}
		else if constexpr (std::is_same_v<D, std::string_view>) {
			return QString::fromUtf8(v.data(), static_cast<int>(v.size()));
		}
		else {
			return QString::fromLatin1(typeid(D).name());
		}
	}

	template<typename... Args>
	void formatLogImplementation(const LoggerName& loggerContext, Logger::LogLevel level,
		const char* fileName,
		const char* functionName,
		unsigned int line,
		const char* fmt,
		Args&&... args)
	{
		QString qfmt = Logger::normalizeFormat(fmt);
		QString qres = qfmt;

		(void)std::initializer_list<int>{
			(qres = qres.arg(toQStringArgFormat(std::forward<Args>(args))), 0)...
		};

		std::string result = qres.toStdString();
		Logger::getInstance()->storeMessage(loggerContext, level, fileName, functionName, line, result);
	}
#endif

#define Info(logger, fmt, ...) formatLogImplementation(logger, Logger::INFO,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(logger, fmt, ...) formatLogImplementation(logger, Logger::DEBUG, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(logger, fmt, ...) formatLogImplementation(logger, Logger::WARNING, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define Error(logger, fmt, ...) formatLogImplementation(logger, Logger::ERRORR, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define InfoIf(condition, logger, fmt, ...) if (condition) formatLogImplementation(logger, Logger::INFO, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define DebugIf(condition, logger, fmt, ...) if (condition) formatLogImplementation(logger, Logger::DEBUG, __FILE__, __func__, __LINE__, fmt,  ##__VA_ARGS__)
#define WarningIf(condition, logger, fmt, ...) if (condition) formatLogImplementation(logger, Logger::WARNING, __FILE__, __func__, __LINE__, fmt,  ##__VA_ARGS__)
#define ErrorIf(condition, logger, fmt, ...) if (condition) formatLogImplementation(logger, Logger::ERRORR, __FILE__, __func__, __LINE__, fmt,  ##__VA_ARGS__)

Q_DECLARE_METATYPE(Logger::T_LOG_MESSAGE)
