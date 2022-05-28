#include <bonjour/bonjourbrowserwrapper.h>

//qt incl
#include <QTimer>
#include <QString>
#include <QStringLiteral>
#include <cstdio>
#include <utils/Logger.h>

// bonjour
#include <bonjour/bonjourservicebrowser.h>

BonjourBrowserWrapper* BonjourBrowserWrapper::instance = nullptr;

const QString DISCOVER_PHILIPS_HUE = QStringLiteral("_hue._tcp");
const QString DISCOVER_WLED = QStringLiteral("_wled._tcp");
const QString DISCOVER_HYPERHDR = QStringLiteral("_hyperhdr-http._tcp");

BonjourBrowserWrapper::BonjourBrowserWrapper(QObject * parent)
	: QObject(parent)
{
	// register meta
	qRegisterMetaType<QMap<QString,BonjourRecord>>("QMap<QString,BonjourRecord>");

	BonjourBrowserWrapper::instance = this;
		
	BonjourServiceBrowser* hue = new BonjourServiceBrowser(this, QLatin1String("_hue._tcp"));
	connect(hue, &BonjourServiceBrowser::currentBonjourRecordsChanged, this, &BonjourBrowserWrapper::foundPhilipsHUE);
	hue->browseForServiceType();

	BonjourServiceBrowser* wled = new BonjourServiceBrowser(this, QLatin1String("_wled._tcp"));
	connect(wled, &BonjourServiceBrowser::currentBonjourRecordsChanged, this, &BonjourBrowserWrapper::foundWLED);
	wled->browseForServiceType();

	BonjourServiceBrowser* hyperhdr = new BonjourServiceBrowser(this, QLatin1String("_hyperhdr-http._tcp"));
	connect(hyperhdr, &BonjourServiceBrowser::currentBonjourRecordsChanged, this, &BonjourBrowserWrapper::foundHyperHDR);
	hyperhdr->browseForServiceType();

}

void BonjourBrowserWrapper::foundHyperHDR(const QList<BonjourRecord> &list)
{
	printf("------------- Found HyperHDR --------------\n");
	_hyperhdrSessions.clear();
	for ( auto rec : list )
	{
		_hyperhdrSessions.insert(rec.serviceName, rec);
		printf("Found HyperHDR: (%s) (%s) (%s) (%s) (%i)\n", QSTRING_CSTR(rec.address), QSTRING_CSTR(rec.serviceName), QSTRING_CSTR(rec.registeredType), QSTRING_CSTR(rec.hostName), (rec.port));
	}	
}

void BonjourBrowserWrapper::foundWLED(const QList<BonjourRecord>& list)
{
	printf("--------------- Found WLED ----------------\n");
	_wledDevices.clear();
	for (auto rec : list)
	{
		_wledDevices.insert(rec.serviceName, rec);
		printf("Found WLED: (%s) (%s) (%s) (%s) (%i)\n", QSTRING_CSTR(rec.address), QSTRING_CSTR(rec.serviceName), QSTRING_CSTR(rec.registeredType), QSTRING_CSTR(rec.hostName), (rec.port));
	}
}

void BonjourBrowserWrapper::foundPhilipsHUE(const QList<BonjourRecord>& list)
{
	printf("------------- Found Hue --------------\n");
	_hueDevices.clear();
	for (auto rec : list)
	{
		_hueDevices.insert(rec.serviceName, rec);
		printf("Found Philips Hue: (%s) (%s) (%s) (%s) (%i)\n", QSTRING_CSTR(rec.address), QSTRING_CSTR(rec.serviceName), QSTRING_CSTR(rec.registeredType), QSTRING_CSTR(rec.hostName), (rec.port));
	}
}
