// bonjour
#include <bonjour/bonjourservicebrowser.h>
#include <bonjour/bonjourbrowserwrapper.h>

//qt incl
#include <QTimer>
#include <QString>
#include <QStringLiteral>
#include <cstdio>
#include <utils/Logger.h>

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

	BonjourServiceBrowser* wled = new BonjourServiceBrowser(this, QLatin1String("_wled._tcp"));
	connect(wled, &BonjourServiceBrowser::currentBonjourRecordsChanged, this, &BonjourBrowserWrapper::foundWLED);

	BonjourServiceBrowser* hyperhdr = new BonjourServiceBrowser(this, QLatin1String("_hyperhdr-http._tcp"));
	connect(hyperhdr, &BonjourServiceBrowser::currentBonjourRecordsChanged, this, &BonjourBrowserWrapper::foundHyperHDR);

	_hueService = hue;
	_wledService = wled;
	_hyperhdrService = hyperhdr;

	QTimer::singleShot(1000, [this]() {if (_hyperhdrService != nullptr) ((BonjourServiceBrowser*)_hyperhdrService)->browseForServiceType(); });
	QTimer::singleShot(1100, [this]() {if (_hueService != nullptr) ((BonjourServiceBrowser*)_hueService)->browseForServiceType(); });
	QTimer::singleShot(1200, [this]() {if (_wledService != nullptr) ((BonjourServiceBrowser*)_wledService)->browseForServiceType(); });
}

const QList<BonjourRecord> BonjourBrowserWrapper::getPhilipsHUE()
{
	QMap<QString, BonjourRecord> copy = _hueDevices;
	QList<BonjourRecord> result;

	for (auto rec : copy.values())
	{
		result.push_back(rec);
	}

	if (_hueService != nullptr)
	{
		((BonjourServiceBrowser*)_hueService)->browseForServiceType();
	}

	return result;
}

const QList<BonjourRecord> BonjourBrowserWrapper::getWLED()
{
	QMap<QString, BonjourRecord> copy = _wledDevices;
	QList<BonjourRecord> result;

	for (auto rec : copy.values())
	{
		result.push_back(rec);
	}

	if (_wledService != nullptr)
	{
		((BonjourServiceBrowser*)_wledService)->browseForServiceType();
	}

	return result;
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
