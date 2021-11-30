#pragma once

#include <QObject>
#include <QString>
#include <memory>

class SysInfo : public QObject
{
	Q_OBJECT

public:
	struct HyperhdrSysInfo
	{
		QString kernelType;
		QString kernelVersion;
		QString architecture;
		QString cpuModelName;
		QString cpuModelType;
		QString cpuRevision;
		QString cpuHardware;
		QString wordSize;
		QString productType;
		QString productVersion;
		QString prettyName;
		QString hostName;
		QString domainName;
		QString qtVersion;
		QString pyVersion;
	};

	static HyperhdrSysInfo get();

private:
	SysInfo();
	void getCPUInfo();

	static std::unique_ptr<SysInfo> _instance;

	HyperhdrSysInfo _sysinfo;
};
