/* bonjourserviceregister.cpp
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

#include <HyperhdrConfig.h>
#include <bonjour/BonjourServiceRegister.h>
#include <bonjour/DiscoveryWrapper.h>
#include <bonjour/BonjourServiceHelper.h>
#include <utils/Logger.h>
#include <utils/GlobalSignals.h>
#include <QTimer>

#define DEFAULT_RETRY 20

BonjourServiceRegister::BonjourServiceRegister(QObject* parent, DiscoveryRecord::Service type, int port) :
	QObject(parent),
	_helper(new BonjourServiceHelper(this, DiscoveryRecord::getmDnsHeader(type), port)),
	_retry(DEFAULT_RETRY)
{
	_serviceRecord.port = port;
	_serviceRecord.type = type;

	connect(_helper, &QThread::finished, this, &BonjourServiceRegister::onThreadExits);
	connect(this, &BonjourServiceRegister::SignalMessageFromFriend, this, &BonjourServiceRegister::messageFromFriendHandler);
	connect(this, &BonjourServiceRegister::SignalIpResolved, this, &BonjourServiceRegister::signalIpResolvedHandler);
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalDiscoveryRequestToScan, this, &BonjourServiceRegister::requestToScanHandler);
}

BonjourServiceRegister::~BonjourServiceRegister()
{
	if (_helper != nullptr)
	{
		disconnect(_helper, &QThread::finished, this, &BonjourServiceRegister::onThreadExits);
		_helper->prepareToExit();
		_helper->quit();
		_helper->wait();
		delete _helper;
		_helper = nullptr;
	}
}

void BonjourServiceRegister::onThreadExits()
{
	if (_helper != nullptr && _retry > 0)
	{
		_retry--;
		QTimer::singleShot(15000, this, [this]() {
			if (_helper != nullptr && _retry > 0)
				_helper->start();
		});
	}
}

void BonjourServiceRegister::registerService()
{
	QTimer::singleShot(1500, [this]() {
		emit GlobalSignals::getInstance()->SignalDiscoveryRequestToScan(DiscoveryRecord::Service::SerialPort);
		if (_helper != nullptr) _helper->start();
	});
}

void BonjourServiceRegister::requestToScanHandler(DiscoveryRecord::Service type)
{
	switch (type)
	{
		case (DiscoveryRecord::Service::HyperHDR): _helper->_scanService |= (1 << DiscoveryRecord::Service::HyperHDR); break;
		case (DiscoveryRecord::Service::WLED): _helper->_scanService |= (1 << DiscoveryRecord::Service::WLED); break;
		case (DiscoveryRecord::Service::PhilipsHue): _helper->_scanService |= (1 << DiscoveryRecord::Service::PhilipsHue); break;
		case (DiscoveryRecord::Service::HomeAssistant): _helper->_scanService |= (1 << DiscoveryRecord::Service::HomeAssistant); break;
		default: break;
	}
}

void BonjourServiceRegister::messageFromFriendHandler(bool isExists, QString mdnsString, QString serverName, int port)
{
	DiscoveryRecord::Service type = DiscoveryRecord::Service::Unknown;
	DiscoveryRecord newRecord;

	if (mdnsString.indexOf(DiscoveryRecord::getmDnsHeader(DiscoveryRecord::Service::WLED)) >= 0)
		type = DiscoveryRecord::Service::WLED;
	else if (mdnsString.indexOf(DiscoveryRecord::getmDnsHeader(DiscoveryRecord::Service::PhilipsHue)) >= 0)
		type = DiscoveryRecord::Service::PhilipsHue;
	else if (mdnsString.indexOf(DiscoveryRecord::getmDnsHeader(DiscoveryRecord::Service::HomeAssistant)) >= 0)
		type = DiscoveryRecord::Service::HomeAssistant;
	else if (mdnsString.indexOf(DiscoveryRecord::getmDnsHeader(DiscoveryRecord::Service::HyperHDR)) >= 0)
		type = DiscoveryRecord::Service::HyperHDR;

	if (type != DiscoveryRecord::Service::Unknown)
	{
		if (serverName.length() > 0 && serverName[serverName.length() - 1] == '.')
		{
			serverName = serverName.left(serverName.length() - 1);
		}

		newRecord.hostName = serverName;
		newRecord.port = port;
		newRecord.type = type;
		newRecord.address = "";
		newRecord.isExists = isExists;
	}

	_result = newRecord;
	_retry = DEFAULT_RETRY;

	resolveIps();
}

void BonjourServiceRegister::signalIpResolvedHandler(QString serverName, QString ip)
{
	if (serverName.length() > 0 && serverName[serverName.length() - 1] == '.')
	{
		serverName = serverName.left(serverName.length() - 1);
	}

	_ips[serverName] = ip;

	resolveIps();
}

void BonjourServiceRegister::resolveIps()
{
	if (_result.type != DiscoveryRecord::Service::Unknown && _result.address.isEmpty())
	{
		for (const QString& k : _ips.keys())
			if (QString::compare(_result.hostName, k, Qt::CaseInsensitive) == 0)
			{
				_result.address = _ips[k];
				break;
			}

		if (!_result.address.isEmpty())
		{
			emit GlobalSignals::getInstance()->SignalDiscoveryEvent(_result);
		}
	}
}
