#include <bonjour/bonjourserviceregister.h>
#include <stdlib.h>

#include <QtCore/QSocketNotifier>
#include <QHostInfo>

#include <utils/Logger.h>
#include <HyperhdrConfig.h>
#include <base/AuthManager.h>

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
	if (_helper != nullptr)
		_helper->start();
}
