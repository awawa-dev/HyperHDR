/* SystemPerformanceCounters.mm
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

#include <performance-counters/SystemPerformanceCounters.h>

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <sys/types.h>
#include <sys/sysctl.h>

mach_msg_type_number_t prevPerfNum = 0U;
processor_info_array_t prevPerfStats = NULL;

void SystemPerformanceCounters::init()
{
	try
	{
		int ret = 0;
		size_t size = sizeof(ret);

		if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) != -1)
		{
		}
		else
			ret = 0;

		if (ret == 0)
		{
			int mib[2];
			size_t len;

			mib[0] = CTL_HW;
			mib[1] = HW_NCPU;
			len = sizeof(cpuCount);

			if (sysctl(mib, 2, &cpuCount, &len, NULL, 0))
				cpuCount = -1;

			mib[0] = CTL_HW;
			mib[1] = HW_MEMSIZE;
			len = sizeof(int64_t);
			if (sysctl(mib, 2, &physicalMemory, &len, NULL, 0))
				physicalMemory = 0;
		}
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

	if (prevPerfStats)
	{				
		size_t prevCpuInfoSize = sizeof(integer_t) * prevPerfNum;
		vm_deallocate(mach_task_self(), (vm_address_t)prevPerfStats, prevCpuInfoSize);
		prevPerfStats = NULL;
	}
}

QString SystemPerformanceCounters::getCPU()
{
	QJsonObject jResult;
	QJsonArray jCores;

	try
	{
		if (!isInitialized)
		{
			init();
			return "";
		}

		if (cpuCount > 0)
		{
			natural_t numberOfCPU = 0U;
			mach_msg_type_number_t currentPerfNum;
			processor_info_array_t currentStats;
			
			if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numberOfCPU, &currentStats, &currentPerfNum) == KERN_SUCCESS)
			{
				if (prevPerfStats)
				{
					QString retVal, retTotal;
					double totUsage = 0, totTotal = 0, valCPU = 0;

					for(int i = 0; i < cpuCount; i++)
					{
						double usage = (
									(currentStats[(CPU_STATE_MAX * i) + CPU_STATE_USER]   - prevPerfStats[(CPU_STATE_MAX * i) + CPU_STATE_USER])
									+ (currentStats[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM] - prevPerfStats[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM])
									+ (currentStats[(CPU_STATE_MAX * i) + CPU_STATE_NICE]   - prevPerfStats[(CPU_STATE_MAX * i) + CPU_STATE_NICE])
									);
						double total = usage + (currentStats[(CPU_STATE_MAX * i) + CPU_STATE_IDLE] - prevPerfStats[(CPU_STATE_MAX * i) + CPU_STATE_IDLE]);

						valCPU = usage / std::max(total, 0.00001);

						jCores.append(valCPU / 100.0f);

						totUsage += usage;
						totTotal += total;
					}

					
					valCPU = (totUsage * 100.0f) / std::max(totTotal, 0.00001);

					valCPU = std::min(std::max(valCPU, 0.0), 100.0);

					jResult["total"] = valCPU;

					jResult["cores"] = jCores;

					size_t prevCpuInfoSize = sizeof(integer_t) * prevPerfNum;
					vm_deallocate(mach_task_self(), (vm_address_t)prevPerfStats, prevCpuInfoSize);					
				}

				prevPerfStats = currentStats;
				prevPerfNum = currentPerfNum;
			}
		}

	}
	catch (...)
	{

	}
	return QJsonDocument(jResult).toJson(QJsonDocument::Compact);
}

QString SystemPerformanceCounters::getRAM()
{
	try
	{
		if (!isInitialized)
		{
			init();
			return "";
		}

		if (physicalMemory > 0)
		{
			vm_size_t pageSize;
			mach_msg_type_number_t count;
			vm_statistics64_data_t vmStats;

			count = sizeof(vmStats) / sizeof(natural_t);
			if (KERN_SUCCESS == host_page_size(mach_host_self(), &pageSize) &&
				KERN_SUCCESS == host_statistics64(mach_host_self(), HOST_VM_INFO,(host_info64_t)&vmStats, &count))
			{
				int64_t usedMemory = ((int64_t)vmStats.active_count +
										 (int64_t)vmStats.inactive_count +
										 (int64_t)vmStats.wire_count) *  (int64_t)pageSize;

				QJsonObject jResult;

				qint64 totalPhysMem = qint64(physicalMemory) / (1024 * 1024);
				jResult["totalPhysMem"] = totalPhysMem;
				qint64 takenMem = qint64(usedMemory) / (1024 * 1024);
				jResult["takenMem"] = takenMem;
				jResult["aspect"] = (qint64) (takenMem * 100) / totalPhysMem;

				return QJsonDocument(jResult).toJson(QJsonDocument::Compact);
			}
		}
	}
	catch (...)
	{

	}
	return "";
}

QString SystemPerformanceCounters::getTEMP()
{
	return "";
}

QString SystemPerformanceCounters::getUNDERVOLATGE()
{
	return "";
}

