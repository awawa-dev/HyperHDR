#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QList>
	#include <QDateTime>
	#include <QJsonObject>
	#include <QString>
	#include <QJsonArray>
	#include <QJsonDocument>

	#include <algorithm>
#endif


#ifdef __linux__
	#define MAXPERFCPU 16
	#define MAXPERFBUFFER 1256
#endif

class SystemPerformanceCounters
{
	private:
		bool isInitialized = false;
		void init();		

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
