#include <bonjour/bonjourserviceregister.h>
#include <HyperhdrConfig.h>
#include <QTimer>

BonjourServiceRegister::BonjourServiceRegister(QObject* parent, const QString& service, int port) :
	QObject(parent),
	_helper(new BonjourServiceHelper(this, service, port))
{
	_serviceRecord.port = port;
	_serviceRecord.serviceName = service;
}

BonjourServiceRegister::~BonjourServiceRegister()
{
	if (_helper != nullptr)
	{
		_helper->prepareToExit();
		_helper->quit();
		_helper->wait();
		delete _helper;
	}
}

void BonjourServiceRegister::registerService()
{
	QTimer::singleShot(1500, [this]() { if (_helper != nullptr) _helper->start(); });
}
