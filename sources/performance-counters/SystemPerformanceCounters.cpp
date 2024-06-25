/* SystemPerformanceCounters.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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
	#include <QTextStream>
	#include <QFile>

	#include <iostream>
	#include <stdlib.h>
	#include <stdio.h>
	#include <vector>	
#endif

#include <utils/InternalClock.h>
#include <performance-counters/SystemPerformanceCounters.h>

#ifdef _WIN32
	#include <windows.h>
	#include <pdh.h>
	#include <pdhmsg.h>
	#pragma comment(lib, "pdh.lib")

	PDH_HQUERY cpuPerfQuery = nullptr;
	PDH_HCOUNTER cpuPerfTotal = nullptr;
#else
	#include <sys/types.h>
	#include <sys/sysinfo.h>
	#include <sys/klog.h>
#endif

#ifdef __linux__

int SystemPerformanceCounters::readCpuLines()
{
	char buffer[MAXPERFBUFFER];
	int numberOfCPU = -1;

	FILE* fp = fopen("/proc/stat", "r");

	if (fp != NULL)
	{
		for (; fgets(buffer, MAXPERFBUFFER, fp) != NULL && numberOfCPU <= MAXPERFCPU; numberOfCPU++)
		{
			int ind = (numberOfCPU + 1) * 4;

			buffer[MAXPERFBUFFER - 1] = 0;

			if (sscanf(buffer, "cp%*s %Lu %Lu %Lu %Lu", &cpuStat[ind], &cpuStat[ind + 1], &cpuStat[ind + 2], &cpuStat[ind + 3]) < 4)
			{
				break;
			}
		}

		fclose(fp);
	}

	return numberOfCPU;
}

void SystemPerformanceCounters::readVoltage()
{
	try
	{
		ssize_t len = klogctl(10, NULL, 0);

		underVoltage = -1;

		if (len > 0)
		{
			std::vector<char> buf(len, 0);

			len = klogctl(3, &buf[0], len);
			if (len > 0)
			{
				buf.resize(len);
				buf[len - 1] = 0;

				if (strstr(buf.data(), "Under-voltage detected!") != NULL || strstr(buf.data(), "Undervoltage detected!") != NULL)
					underVoltage = 1;
				else
					underVoltage = 0;

				voltageTimeStamp = InternalClock::now() / 1000;
			}
		}
	}
	catch (...)
	{
		underVoltage = -1;
	}
}
#endif

void SystemPerformanceCounters::init()
{
	try
	{
#ifdef _WIN32

		if (PdhOpenQuery(NULL, NULL, &cpuPerfQuery) == ERROR_SUCCESS)
		{
#ifdef UNICODE
			PdhAddEnglishCounter(cpuPerfQuery, L"\\Processor(*)\\% Processor Time", NULL, &cpuPerfTotal);
#else
			PdhAddEnglishCounter(cpuPerfQuery, "\\Processor(*)\\% Processor Time", NULL, &cpuPerfTotal);
#endif
			PdhCollectQueryData(cpuPerfQuery);
		}

#else

		totalPerfCPU = readCpuLines();

		underVoltage = -1;
		voltageTimeStamp = -1;

		QFile file("/etc/rpi-issue");
		if (file.exists() && file.open(QFile::ReadOnly | QFile::Text))
		{
			QTextStream in(&file);
			QString sysInfo = in.readAll();
			if (sysInfo.indexOf("Raspberry Pi reference", Qt::CaseInsensitive) >= 0)
			{
				readVoltage();
			}
			file.close();
		}


#endif		
	}
	catch (...)
	{

	}
	isInitialized = true;
}

SystemPerformanceCounters::~SystemPerformanceCounters()
{
	if (!isInitialized)
		return;

#ifdef _WIN32
	if (cpuPerfQuery != nullptr)
		PdhRemoveCounter(cpuPerfQuery);
	cpuPerfQuery = nullptr;
#endif
}

QString SystemPerformanceCounters::getCPU()
{
	try
	{
		QJsonObject jResult;
		QJsonArray jCores;

		if (!isInitialized)
		{
			init();
			return "";
		}

#ifdef _WIN32
		DWORD size = 0, count = 0;

		if (cpuPerfQuery == nullptr || PdhCollectQueryData(cpuPerfQuery) != ERROR_SUCCESS)
			return "";

		if (PdhGetFormattedCounterArray(cpuPerfTotal, PDH_FMT_LONG, &size, &count, nullptr) != PDH_MORE_DATA)
			return "";

		QByteArray myByteArray(size, 0);
		PDH_FMT_COUNTERVALUE_ITEM* bufferData = (PDH_FMT_COUNTERVALUE_ITEM*)myByteArray.data();

		if (PdhGetFormattedCounterArray(cpuPerfTotal, PDH_FMT_LONG, &size, &count, bufferData) != ERROR_SUCCESS)
		{
			return "";
		}

		for (DWORD i = 0; i < count; i++)
		{
			auto valCPU = bufferData[i].FmtValue.longValue;

#ifdef UNICODE
			QString convertedStr = QString::fromWCharArray(bufferData[i].szName);
#else
			QString convertedStr = QString::fromLocal8Bit(buffer[i].szName);
#endif

			if (convertedStr != "_Total")
			{
				jCores.append(valCPU / 100.0f);
			}
			else
			{
				jResult["total"] = (double)valCPU;
			}
		}

#else

		if (totalPerfCPU > 0)
		{
			memcpy(&cpuOldStat, &cpuStat, sizeof(cpuOldStat));

			if (readCpuLines() == totalPerfCPU)
			{
				for (int i = -1; i < totalPerfCPU; i++)
				{
					int j = (i + 1) * 4;

					double valCPU = 0;
					unsigned long long& totalUser = cpuStat[j + 0], & totalUserLow = cpuStat[j + 1], & totalSys = cpuStat[j + 2], & totalIdle = cpuStat[j + 3];
					unsigned long long& lastTotalUser = cpuOldStat[j + 0], & lastTotalUserLow = cpuOldStat[j + 1], & lastTotalSys = cpuOldStat[j + 2], & lastTotalIdle = cpuOldStat[j + 3];

					if (!(totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
						totalSys < lastTotalSys || totalIdle < lastTotalIdle))
					{
						unsigned long long total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) +
							(totalSys - lastTotalSys);
						valCPU = total;
						total += (totalIdle - lastTotalIdle);
						if (total != 0)
							valCPU /= total;
						else
							valCPU = 0;
						valCPU *= 100;
					}

					valCPU = std::min(std::max(valCPU, 0.0), 100.0);

					if (i >= 0)
					{
						jCores.append(valCPU / 100.0f);
					}
					else
					{
						jResult["total"] = valCPU;
					}
				}
			}
		}

#endif

		jResult["cores"] = jCores;
		return QJsonDocument(jResult).toJson(QJsonDocument::Compact);
	}
	catch (...)
	{

	}
	return "";
}

QString SystemPerformanceCounters::getRAM()
{
	try
	{
		QJsonObject jResult;

		if (!isInitialized)
		{
			init();
			return "";
		}

#ifdef _WIN32

		MEMORYSTATUSEX memInfo{};
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		if (GlobalMemoryStatusEx(&memInfo) == 0)
			return "";
		qint64 totalPhysMem = qint64(memInfo.ullTotalPhys) / (1024ll * 1024);
		qint64 physMemAv = qint64(memInfo.ullAvailPhys) / (1024ll * 1024);
		qint64 takenMem = totalPhysMem - physMemAv;
		qint64 aspect = (takenMem * 100) / totalPhysMem;

#else

		struct sysinfo memInfo;
		sysinfo(&memInfo);

		long long totalPhysMem = memInfo.totalram;
		totalPhysMem *= memInfo.mem_unit;

		long long physMemAv = memInfo.freeram + memInfo.bufferram;
		physMemAv *= memInfo.mem_unit;

		unsigned long available = 0, total = 0, free = 0;
		FILE* pFile = fopen("/proc/meminfo", "r");
		if (pFile != NULL)
		{
			if (fscanf(pFile, "MemTotal: %lu kB\n", &total) == 1 &&
				fscanf(pFile, "MemFree: %lu kB\n", &free) == 1 &&
				fscanf(pFile, "MemAvailable: %lu kB\n", &available) == 1)
			{
				physMemAv = available * 1024;
				totalPhysMem = total * 1024;
			}
			fclose(pFile);
		}

		totalPhysMem /= (1024 * 1024);
		physMemAv /= (1024 * 1024);

		long long takenMem = totalPhysMem - physMemAv;
		qint64 aspect = (takenMem * 100) / totalPhysMem;		
#endif
		jResult["totalPhysMem"] = (qint64) totalPhysMem;
		jResult["takenMem"] = (qint64)takenMem;
		jResult["aspect"] = (qint64) aspect;

		return QJsonDocument(jResult).toJson(QJsonDocument::Compact);
	}
	catch (...)
	{

	}
	return "";
}

QString SystemPerformanceCounters::getTEMP()
{
	QString result;

	try
	{
#ifdef __linux__

		FILE* fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");

		if (fp != NULL)
		{
			long long int temp;

			if (fscanf(fp, "%lli", &temp) >= 1)
			{
				double tempVal = temp / 1000.0f;
				QString color = (tempVal <= 65) ? "stats-report-ok" : ((tempVal <= 75) ? "stats-report-warning" : "stats-report-error");
				result = QString("<span class='%1 fw-bold'>%2</span>").arg(color).arg(QString::number(tempVal, 'f', 2));
			}

			fclose(fp);
		}
#endif
	}
	catch (...)
	{

	}

	return result;
}

QString SystemPerformanceCounters::getUNDERVOLATGE()
{
	try
	{
		if (!isInitialized)
		{
			init();
			return "";
		}

#ifdef __linux__
		QString current = "";
		if (voltageTimeStamp > 0 && (InternalClock::now() / 1000) - voltageTimeStamp >= 60)
		{
			readVoltage();
			current = "NOW";
		}

		if (underVoltage >= 0)
			return QString::number(underVoltage) + current;
#endif

	}
	catch (...)
	{

	}

	return "";
}
