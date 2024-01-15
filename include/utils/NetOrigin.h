#pragma once

#ifndef PCH_ENABLED
	#include <QHostAddress>
	#include <QJsonArray>

	#include <utils/Logger.h>
	#include <utils/settings.h>
#endif

class NetOrigin : public QObject
{
	Q_OBJECT

public:
	bool accessAllowed(const QHostAddress& address, const QHostAddress& local);
	static bool isLocalAddress(const QHostAddress& address, const QHostAddress& local);
	
private slots:
	void settingsChangedHandler(settings::type type, const QJsonDocument& config);

private:
	friend class HyperHdrDaemon;
	NetOrigin(QObject* parent = nullptr, Logger* log = Logger::getInstance("NETWORK"));

	Logger* _log;
	bool _internetAccessAllowed;
	QList<QHostAddress> _ipWhitelist;
	QMutex _mutex;
};
