/* PerformanceCounters.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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

#include <utils/PerformanceCounters.h>
#include <utils/Logger.h>
#include <QFile>
#include <QJsonArray>
#include <QMutableListIterator>
#include <QTextStream>

std::unique_ptr<PerformanceCounters> PerformanceCounters::_instance = nullptr;

PerformanceReport::PerformanceReport(int _type, qint64 _token, QString _name, double _param1, qint64 _param2, qint64 _param3, qint64 _param4, int _id)
{
	PerformanceReportType _testType = PerformanceReportType::UNKNOWN;

	switch (_type)
	{
		case static_cast<int>(PerformanceReportType::VIDEO_GRABBER):
		case static_cast<int>(PerformanceReportType::INSTANCE):
		case static_cast<int>(PerformanceReportType::LED):
		case static_cast<int>(PerformanceReportType::CPU_USAGE):
		case static_cast<int>(PerformanceReportType::RAM_USAGE):
		case static_cast<int>(PerformanceReportType::CPU_TEMPERATURE):
		case static_cast<int>(PerformanceReportType::SYSTEM_UNDERVOLTAGE):
			_testType = static_cast<PerformanceReportType>(_type);
			break;
	}

	type = static_cast<int>(_testType);
	token = _token;
	name = _name;
	param1 = _param1;
	param2 = _param2;
	param3 = _param3;
	param4 = _param4;
	id = _id;
	timeStamp = InternalClock::now() / 1000;
};

PerformanceCounters::PerformanceCounters()
{
	qRegisterMetaType<PerformanceReport>();

	_lastRead = 0;

	try
	{		
		_log = Logger::getInstance("PERFORMANCE");
	}
	catch (...)
	{
		Debug(_log, "Failed to initialized");
	}

	connect(this, &PerformanceCounters::newCounter, this, &PerformanceCounters::receive);
	connect(this, &PerformanceCounters::removeCounter, this, &PerformanceCounters::remove);
	connect(this, &PerformanceCounters::triggerBroadcast, this, &PerformanceCounters::broadcast);
	connect(this, &PerformanceCounters::performanceInfoRequest, this, &PerformanceCounters::request);
}

void PerformanceCounters::request(bool all)
{
	if (InternalClock::now() - _lastRead < 980)
		return;

	_lastRead = InternalClock::now();

	QString cpu = _system.getCPU();
	if (cpu != "")
	{
		PerformanceReport pr;
		pr.type = static_cast<int>(PerformanceReportType::CPU_USAGE);
		pr.name = cpu;
		createUpdate(pr);
	}

	QString ram = _system.getRAM();
	if (ram != "")
	{
		PerformanceReport pr;
		pr.type = static_cast<int>(PerformanceReportType::RAM_USAGE);
		pr.name = ram;
		createUpdate(pr);
	}

	QString temp = _system.getTEMP();
	if (temp != "")
	{
		PerformanceReport pr;
		pr.type = static_cast<int>(PerformanceReportType::CPU_TEMPERATURE);
		pr.name = temp;
		createUpdate(pr);
	}

	QString under = _system.getUNDERVOLATGE();
	if (under != "")
	{
		PerformanceReport pr;
		pr.type = static_cast<int>(PerformanceReportType::SYSTEM_UNDERVOLTAGE);
		
		if (under.indexOf("NOW") >= 0 )
		{
			under = under.replace("NOW", "");
			if (under == "1")
				Error(_log, "Under-voltage detected. The system is not stable. Please fix it!");
		}
		pr.name = under;

		createUpdate(pr);
	}
}

PerformanceCounters* PerformanceCounters::getInstance()
{
	if (PerformanceCounters::_instance == nullptr)
		PerformanceCounters::_instance = std::unique_ptr<PerformanceCounters>(new PerformanceCounters());

	return PerformanceCounters::_instance.get();
}

qint64 PerformanceCounters::currentToken()
{
	return (InternalClock::now() / 1000) / (qint64)60;
}

void PerformanceCounters::receive(PerformanceReport pr)
{
	QMutableListIterator<PerformanceReport> cleaner(this->_reports);
	qint64 now = InternalClock::now() / 1000;
		
	while (cleaner.hasNext())
	{
		PerformanceReport del = cleaner.next();
		if ((del.type == pr.type &&
			del.id == pr.id) || (now - del.timeStamp) >= ((del.token>0) ? 65: 125))
		{
			if (del.type != pr.type || del.id != pr.id)
				deleteUpdate(del.type, del.id);

			cleaner.remove();			
		}
	}

	bool _inserted = false;

	for (auto ins = this->_reports.begin(); ins != this->_reports.end(); ins++)		
		if ((*ins).id > pr.id || ((*ins).id == pr.id && (*ins).type >= pr.type))
		{
			this->_reports.insert(ins, pr);
			_inserted = true;
			break;
		}		

	if (!_inserted)
		this->_reports.append(pr);
			
	consoleReport(pr.type, pr.token);

	createUpdate(pr);
}

void PerformanceCounters::remove(int type, int id)
{	
	QMutableListIterator<PerformanceReport> i(this->_reports);
	int token = -1;

	while (i.hasNext())
	{
		PerformanceReport del = i.next();
		if (del.type == type &&
			del.id == id)
		{
			token = del.token;
			i.remove();
		}
	}
	consoleReport(type, token);

	deleteUpdate(type, id);
}

void PerformanceCounters::consoleReport(int type, int token)
{
	if (type == static_cast<int>(PerformanceReportType::VIDEO_GRABBER) ||
		type == static_cast<int>(PerformanceReportType::INSTANCE) ||
		type == static_cast<int>(PerformanceReportType::LED))
	{
		for (auto z : this->_reports)
			if (z.token != token && (
				z.type == static_cast<int>(PerformanceReportType::VIDEO_GRABBER) ||
				z.type == static_cast<int>(PerformanceReportType::INSTANCE) ||
				z.type == static_cast<int>(PerformanceReportType::LED)))
				return;
	}
	else
		return;

	QStringList list;
	QMutableListIterator<PerformanceReport> i(this->_reports);

	while (i.hasNext())
	{
		PerformanceReport del = i.next();
		if (del.type == static_cast<int>(PerformanceReportType::VIDEO_GRABBER))
		{
			if (del.token > 0)
				list.append(QString("[USB capturing: FPS = %1, decoding = %2ms, frames = %3, invalid = %4]").arg(del.param1, 0, 'f', 2).arg(del.param2).arg(del.param3).arg(del.param4));
		}
		else if (del.type == static_cast<int>(PerformanceReportType::INSTANCE))
		{
			if (del.token > 0)
				list.append(QString("[INSTANCE%1: FPS = %2, processed = %3]").arg(del.id).arg(del.param1, 0, 'f', 2).arg(del.param2));
		}
		else if (del.type == static_cast<int>(PerformanceReportType::LED))
		{
			if (del.token > 0)
				list.append(QString("[LED%1: FPS = %2, send = %3, processed = %4]").arg(del.id).arg(del.param1, 0, 'f', 2).arg(del.param2).arg(del.param3));
		}
	}

	if (list.count() > 0)	
		Info(_log, "%s", QSTRING_CSTR(list.join(", ")));

}

void PerformanceCounters::createUpdate(PerformanceReport pr)
{
	QJsonObject report;
	qint64 helper = InternalClock::now() / 1000;

	report["remove"] = 0;
	report["type"] = pr.type;
	report["token"] = pr.token;
	report["name"] = pr.name;
	report["param1"] = pr.param1;
	report["param2"] = pr.param2;
	report["param3"] = pr.param3;
	report["param4"] = pr.param4;
	report["id"] = pr.id;
	if (pr.token > 0)
		report["refresh"] = 60 - (helper % 60);
	else
		report["refresh"] = qMax(1 - (helper / 60 - pr.timeStamp / 60), 0ll) * 60 + 60 - (helper % 60);

	emit performanceUpdate(report);
}

void PerformanceCounters::deleteUpdate(int type, int id)
{
	QJsonObject report;

	report["remove"] = 1;
	report["type"] = type;
	report["id"] = id;

	emit performanceUpdate(report);
}

void PerformanceCounters::broadcast()
{
	QJsonArray arr;
	for (auto pr : this->_reports)
	{
		QJsonObject report;
		qint64 helper = InternalClock::now() / 1000;

		report["remove"] = 0;
		report["type"] = pr.type;
		report["token"] = pr.token;
		report["name"] = pr.name;
		report["param1"] = pr.param1;
		report["param2"] = pr.param2;
		report["param3"] = pr.param3;
		report["param4"] = pr.param4;
		report["id"] = pr.id;

		if (pr.token > 0)
			report["refresh"] = 60 - (helper % 60);
		else
			report["refresh"] = qMax(1 - (helper / 60 - pr.timeStamp / 60), 0ll) * 60 + 60 - (helper % 60);

		arr.append(report);		
	}

	QJsonObject report;

	report["broadcast"] = arr;

	emit performanceUpdate(report);
}
