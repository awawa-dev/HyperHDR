#ifndef BONJOURSERVICEREGISTER_H
#define BONJOURSERVICEREGISTER_H

#include <QtCore/QObject>
#include <QHash>

#include <bonjour/DiscoveryRecord.h>
#include <bonjour/bonjourservicehelper.h>

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
	void resolveIpHandler(QString serverName, QString ip);
	void onThreadExits();

signals:
	void messageFromFriend(bool isWelcome, QString mdnsString, QString serverName, int port);
	void resolveIp(QString serverName, QString ip);

private:
	void resolveIps();

	BonjourServiceHelper* _helper;
	DiscoveryRecord		_serviceRecord;
	QHash<QString, QString> _ips;
	DiscoveryRecord		_result;
};

#endif // BONJOURSERVICEREGISTER_H
