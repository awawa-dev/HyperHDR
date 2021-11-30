#pragma once

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QJsonObject>
#include <memory>
#include <utils/SystemPerformanceCounters.h>

class Logger;

enum class PerformanceReportType {VIDEO_GRABBER = 1, INSTANCE = 2, LED = 3, CPU_USAGE = 4, RAM_USAGE = 5, CPU_TEMPERATURE = 6, SYSTEM_UNDERVOLTAGE = 7, UNKNOWN = 8};

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

	PerformanceReport(int _type, qint64 _token, QString _name, double _param1, qint64 _param2, qint64 _param3, qint64 _param4, int _id = -1);

	PerformanceReport(const PerformanceReport& ) = default;

	PerformanceReport() = default;
};

Q_DECLARE_METATYPE(PerformanceReport);

class PerformanceCounters : public QObject
{
	Q_OBJECT

	void consoleReport(int type, int token);
	void createUpdate(PerformanceReport pr);
	void deleteUpdate(int type, int id);
	void broadcast();

	static std::unique_ptr<PerformanceCounters> _instance;

	Logger*	_log;
	QList<PerformanceReport> _reports;
	SystemPerformanceCounters _system;

public:

	PerformanceCounters();

	static PerformanceCounters* getInstance();
	static qint64 currentToken();

private slots:

	void receive(PerformanceReport pr);
	void remove(int type, int id);
	void request(bool all);

signals:

	void newCounter(PerformanceReport pr);
	void removeCounter(int type, int id);
	void performanceUpdate(QJsonObject report);
	void triggerBroadcast();
	void performanceInfoRequest(bool all);
};

