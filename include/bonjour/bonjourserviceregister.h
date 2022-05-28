#ifndef BONJOURSERVICEREGISTER_H
#define BONJOURSERVICEREGISTER_H

#include <QtCore/QObject>

#include <bonjour/bonjourrecord.h>
#include <bonjour/bonjourservicehelper.h>

class BonjourServiceRegister : public QObject
{
    Q_OBJECT

public:
    BonjourServiceRegister(QObject *parent, const QString& service, int port);
    ~BonjourServiceRegister() override;

    void registerService();

public slots:
	int getPort()
	{
		return _serviceRecord.port;
	};

private:
	BonjourServiceHelper* _helper;
	BonjourRecord		_serviceRecord;
	bool				_isActive;
};

#endif // BONJOURSERVICEREGISTER_H
