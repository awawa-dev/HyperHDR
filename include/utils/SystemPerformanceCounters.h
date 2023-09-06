#pragma once

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <algorithm>

#ifdef __linux__
	#define MAXPERFCPU 16
	#define MAXPERFBUFFER 1256
#endif

class SystemPerformanceCounters
{
	private:
		bool isInitialized = false;
		void init();
		QString getChar(double val)
		{
			auto scale = qMax(qMin(qRound(val * 20), 20), 1);

			QString color = (val <= 0.5) ? "cpu_low_usage" : ((val <= 0.8) ? "cpu_medium_usage" : "cpu_high_usage"); //"ForestGreen" : ((val <= 0.8) ? "orange" : "red");
			QString box = QString("<rect x='0' y='%1' width='12' height='%2' />").arg(20 - scale).arg(scale);
			return QString("<svg width='12' height='20' viewBox='0 0 10 20' class='specialchar %1'>%2</svg>").arg(color).arg(box);
		};

	#ifdef __linux__
		int underVoltage = -1;
		qint64 voltageTimeStamp = -1;
		int totalPerfCPU = 0;
		unsigned long long  int cpuStat[(MAXPERFCPU + 2) * 4];
		unsigned long long  int cpuOldStat[(MAXPERFCPU + 2) * 4];

		int readCpuLines();
		void readVoltage();
	#endif

	#ifdef __APPLE__
		int cpuCount = -1;
		int64_t physicalMemory = -1;
	#endif

	public:
		~SystemPerformanceCounters();

		QString getCPU();
		QString getRAM();
		QString getTEMP();
		QString getUNDERVOLATGE();
};
