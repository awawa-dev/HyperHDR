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
			auto m= 8.0f;
			if (val > 7 / m)
				return "<span class='specialchar' style='color:red;'>&#9607</span>";
			else if (val > 6 / m)
				return "<span class='specialchar' style='color:orange;'>&#9607</span>";
			else if (val > 5 / m)
				return "<span class='specialchar' style='color:orange;'>&#9606</span>";
			else if (val > 4 / m)
				return "<span class='specialchar' style='color:ForestGreen;'>&#9605</span>";
			else if (val > 3 / m)
				return "<span class='specialchar' style='color:ForestGreen;'>&#9603</span>";
			else if (val > 2 / m)
				return "<span class='specialchar' style='color:ForestGreen;'>&#9603</span>";
			else if (val > 1 / m)
				return "<span class='specialchar' style='color:ForestGreen;'>&#9602</span>";
			else
				return "<span class='specialchar' style='color:ForestGreen;'>&#9601</span>";
		}

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
