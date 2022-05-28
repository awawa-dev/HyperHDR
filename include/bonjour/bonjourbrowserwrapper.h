#pragma once
// qt incl
#include <QObject>
#include <QMap>
#include <QHostInfo>

#include <bonjour/bonjourrecord.h>

class BonjourServiceBrowser;
class QTimer;

class BonjourBrowserWrapper : public QObject
{
	Q_OBJECT
private:
	friend class HyperHdrDaemon;
	///
	/// @brief Browse for hyperhdr services in bonjour, constructed from HyperHDRDaemon
	///        Searching for hyperhdr http service by default
	///
	BonjourBrowserWrapper(QObject * parent = nullptr);

public:

	///
	/// @brief Get all available sessions
	///
	QMap<QString, BonjourRecord> getAllServices() { return _hyperhdrSessions; }

	static BonjourBrowserWrapper* instance;
	static BonjourBrowserWrapper *getInstance()	{ return instance; }

signals:
	///
	/// @brief Emits whenever a change happend
	///
	void browserChange( const QMap<QString, BonjourRecord> &bRegisters );

private:
	// contains all current active service sessions
	QMap<QString, BonjourRecord> _hyperhdrSessions, _wledDevices, _hueDevices;
	
private slots:
	///
	/// @brief is called whenever a BonjourServiceBrowser emits change
	/// 
	void foundHyperHDR(const QList<BonjourRecord>& list);
	void foundWLED(const QList<BonjourRecord>& list);
	void foundPhilipsHUE(const QList<BonjourRecord>& list);
};
