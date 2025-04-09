#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QList>
	#include <QDateTime>
	#include <QJsonObject>

	#include <memory>
#endif

class Logger;

namespace hyperhdr
{
	enum PerformanceReportType { VIDEO_GRABBER = 1, INSTANCE = 2, LED = 3, CPU_USAGE = 4, RAM_USAGE = 5, CPU_TEMPERATURE = 6, SYSTEM_UNDERVOLTAGE = 7, UNKNOWN = 8 };
}

struct PerformanceReport
{
	int		type;
	int		id;
	QString name;
	double	param1;
	qint64	param2;
	qint64	param3;
	qint64	param4;
	qint64	timeStamp;
	qint64	token;

	PerformanceReport();

	PerformanceReport(int _type, qint64 _token, QString _name, double _param1, qint64 _param2, qint64 _param3, qint64 _param4, int _id = -1);

	PerformanceReport(const PerformanceReport&) = default;
};

Q_DECLARE_METATYPE(PerformanceReport);

class SystemPerformanceCounters;

class PerformanceCounters : public QObject
{
	Q_OBJECT

private:
	void consoleReport(int type, int token);
	void createUpdate(PerformanceReport pr);
	void deleteUpdate(int type, int id);

	Logger* _log;
	QList<PerformanceReport> _reports;
	std::unique_ptr<SystemPerformanceCounters> _system;
	qint64 _lastRead;
	qint64 _lastNetworkScan;

public:

	PerformanceCounters();
	~PerformanceCounters();

	static qint64 currentToken();

private slots:

	void signalPerformanceNewReportHandler(PerformanceReport pr);
	void signalPerformanceStateChangedHandler(bool state, hyperhdr::PerformanceReportType type, int id, QString name = "");
	

public slots:
	void triggerBroadcast();
	void performanceInfoRequest(bool all);

signals:
	void SignalPerformanceStatisticsUpdated(QJsonObject report);
};

