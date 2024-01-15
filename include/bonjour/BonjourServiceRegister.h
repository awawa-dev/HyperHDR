#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QHash>
#endif

#include <bonjour/DiscoveryRecord.h>

class BonjourServiceHelper;

class BonjourServiceRegister : public QObject
{
	Q_OBJECT

public:
	BonjourServiceRegister(QObject* parent, DiscoveryRecord::Service type, int port);
	~BonjourServiceRegister() override;

	void registerService();

public slots:
	int getPort()
	{
		return _serviceRecord.port;
	};

	void requestToScanHandler(DiscoveryRecord::Service type);
	void messageFromFriendHandler(bool isExists, QString mdnsString, QString serverName, int port);
	void signalIpResolvedHandler(QString serverName, QString ip);
	void onThreadExits();

signals:
	void SignalMessageFromFriend(bool isWelcome, QString mdnsString, QString serverName, int port);
	void SignalIpResolved(QString serverName, QString ip);

private:
	void resolveIps();

	BonjourServiceHelper* _helper;
	DiscoveryRecord		_serviceRecord;
	QHash<QString, QString> _ips;
	DiscoveryRecord		_result;
	int _retry;
};
