#pragma once

#include <QHash>
#include <bonjour/bonjourservicehelper.h>
class BonjourRecord;

class BonjourServiceBrowser: public QObject
{
	Q_OBJECT

public:
	BonjourServiceBrowser(QObject* parent, QString service);

	~BonjourServiceBrowser();

	void browseForServiceType();

public slots:
	void resolveHandler(QString serverName, int port);
	void resolveHandlerIp(QString serverName, QString ip);

signals:
	void currentBonjourRecordsChanged(const QList<BonjourRecord>& list);
	void resolve(QString serverName, int port);
	void resolveIp(QString serverName, QString ip);

private:
	void resolveIps();

	BonjourServiceHelper* _helper;

	QString				_service;

	QList<BonjourRecord> _result;
	QHash<QString,QString> _ips;
};

