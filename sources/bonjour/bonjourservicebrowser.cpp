#include <bonjour/bonjourrecord.h>
#include <bonjour/bonjourservicebrowser.h>

int BonjourServiceHelper::_instaceCount = 0;

BonjourServiceBrowser::BonjourServiceBrowser(QObject* parent, QString service) :
	QObject(parent),
	_helper(new BonjourServiceHelper(this, service)),
	_service(service)
{
	connect(this, &BonjourServiceBrowser::resolve, this, &BonjourServiceBrowser::resolveHandler);
	connect(this, &BonjourServiceBrowser::resolveIp, this, &BonjourServiceBrowser::resolveHandlerIp);
};


void BonjourServiceBrowser::resolveHandler(QString serverName, int port)
{
	BonjourRecord br;

	br.hostName = serverName;
	br.port = port;
	br.serviceName = _service;
	_result.append(br);

	resolveIps();
}

void BonjourServiceBrowser::resolveHandlerIp(QString serverName, QString ip)
{
	_ips[serverName] = ip;

	resolveIps();
}

void BonjourServiceBrowser::resolveIps()
{
	QList<BonjourRecord> result;
	bool changed = false;

	for (BonjourRecord& r : _result)
		if (r.address.isEmpty())
		{
			for (QString& k : _ips.keys())
				if (QString::compare(r.hostName, k, Qt::CaseInsensitive) == 0)
				{
					r.address = _ips[k];
					result.append(r);
					changed = true;
				}
		}
		else
			result.append(r);

	if (changed)
		emit currentBonjourRecordsChanged(result);
}


BonjourServiceBrowser::~BonjourServiceBrowser()
{
	if (_helper != nullptr)
	{
		_helper->quit();
		_helper->wait();
		delete _helper;
	}
};

void BonjourServiceBrowser::browseForServiceType()
{
	if (_helper != nullptr)
	{
		_result.clear();
		_ips.clear();
		_helper->start();
	}
};
