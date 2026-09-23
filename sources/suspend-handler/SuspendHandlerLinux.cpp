/* SuspendHandlerLinux.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <unistd.h>
#include <QLatin1String>
#include <base/HyperHdrManager.h>
#include <image/Image.h>
#include <suspend-handler/SuspendHandlerLinux.h>
#include <suspend-handler/SuspendHandlerLinuxDBus.h>
#include <utils/Components.h>

namespace
{
	constexpr QLatin1String ActiveChanged{ "ActiveChanged" };
	constexpr QLatin1String GnomeService{ "org.gnome.ScreenSaver" };
	constexpr QLatin1String GnomePath{ "/org/gnome/ScreenSaver" };
	constexpr QLatin1String KdeService{ "org.freedesktop.ScreenSaver" };
	constexpr QLatin1String KdePath{ "/org/freedesktop/ScreenSaver" };
	constexpr QLatin1String XfceService{ "org.xfce.ScreenSaver" };
	constexpr QLatin1String XfcePath{ "/org/xfce/ScreenSaver" };
	constexpr QLatin1String PrepareForSleep{ "PrepareForSleep" };
	constexpr QLatin1String PropertiesChanged{ "PropertiesChanged" };
	constexpr QLatin1String Login1Service{ "org.freedesktop.login1" };
	constexpr QLatin1String Login1Path{ "/org/freedesktop/login1" };
	constexpr QLatin1String Login1Interface{ "org.freedesktop.login1.Manager" };
}

SessionMonitorDBus::SessionMonitorDBus(QObject* parent) : HelperDBus(parent) {
	connect(this, &HelperDBus::signalReceived, this, &SessionMonitorDBus::handleSignal, Qt::QueuedConnection);
};

bool SessionMonitorDBus::open() {
	if (!HelperDBus::open(DBUS_BUS_SESSION))
		return false;

	if (!(addSignalMatch(GnomeService, GnomePath, GnomeService, ActiveChanged) | addSignalMatch(KdeService, KdePath, KdeService, ActiveChanged) | addSignalMatch(XfceService, XfcePath, XfceService, ActiveChanged)))
	{
		closeConnection();
		return false;
	}

	return true;
};

void SessionMonitorDBus::handleSignal(const QString&, const QString&, const QString&, const QVariantList& arguments, bool parseError) {
	if (parseError || arguments.size() < 1 || !HelperDBus::isType<bool>(arguments[0]))
		return;

	bool monitorOff = arguments[0].toBool();

	qDebug().nospace() << "Display event: monitor is " << (monitorOff ? "OFF" : "ON");
	emit monitorStateChanged(!monitorOff, hyperhdr::SystemComponent::MONITOR);
}

SystemSuspendDBus::SystemSuspendDBus(QObject* parent) : HelperDBus(parent) {
	connect(this, &HelperDBus::signalReceived, this, &SystemSuspendDBus::handleSignal, Qt::QueuedConnection);
}

bool SystemSuspendDBus::open(bool sessionLocker) {
	if (!HelperDBus::open(DBUS_BUS_SYSTEM))
		return false;

	if (!addSignalMatch(Login1Service, Login1Path, Login1Interface, PrepareForSleep)) {
		closeConnection();
		return false;
	}

	if (sessionLocker) {
		_sessionPath = getSessionPath();
		if (_sessionPath.isEmpty() || !addSignalMatch(Login1Service, _sessionPath, DBusProperties, PropertiesChanged)) {
			_sessionPath.clear();
			qWarning() << "SystemSuspendDBus: cannot watch Session.PropertiesChanged";
		}
	}

	return true;
}

QString SystemSuspendDBus::getSessionPath() {
	QString userPath = QStringLiteral("/org/freedesktop/login1/user/_%1").arg(getuid());

	const QVariant value = getProperty(Login1Service, userPath, QStringLiteral("org.freedesktop.login1.User"),
										QStringLiteral("Display"),"User.Display");

	if (isType<QVariantMap>(value)) {
		if (const QVariant session = value.toMap().value(QStringLiteral("1")); isType<QString>(session))
			return session.toString();
	}

	qDebug() << "SystemSuspendDBus: cannot read sessionPath";
	return {};
}

std::optional<bool> SystemSuspendDBus::getLockHint(const QString& sessionPath) {
	std::optional<bool> lockHint;

	const QVariant value = getProperty(Login1Service, sessionPath, QStringLiteral("org.freedesktop.login1.Session"),
										QStringLiteral("LockedHint"), "Session.LockedHint");

	if (isType<bool>(value)) {
		lockHint = value.toBool();
		qDebug() << "SystemSuspendDBus: lockHint =" << lockHint.value();
	}

	return lockHint;
}

void SystemSuspendDBus::handleSignal(const QString& path, const QString& interface, const QString& member, const QVariantList& arguments, bool parseError) {
	if (!parseError && interface == HelperDBus::DBusProperties && member == PropertiesChanged &&
		arguments.size() >= 2 && isType<QString>(arguments[0]) && isType<QVariantMap>(arguments[1]) && arguments[0].toString() == "org.freedesktop.login1.Session")
	{
		if (const QVariant value = arguments[1].toMap().value("LockedHint"); isType<bool>(value)) {
			const bool lockHint = value.toBool();
			qDebug() << "SystemSuspendDBus: LockedHint changed =" << lockHint;

			if (_delayedWakeup && !lockHint) {
				_delayedWakeup = false;
				qDebug().nospace() << "OS event: waking up (delayed)";
				emit prepareForSleep(true, hyperhdr::SystemComponent::SUSPEND);
			}
		}
	}
	else if (!parseError && interface == Login1Interface && member == PrepareForSleep &&
			arguments.size() && HelperDBus::isType<bool>(arguments[0]))
	{
		if (bool isGoingSleep = arguments[0].toBool(); !isGoingSleep && !_sessionPath.isEmpty() && getLockHint(_sessionPath) == true)
		{
			_delayedWakeup = true;
			qDebug() << "SystemSuspendDBus: skipping resume from sleep for now — the session is locked";
		}
		else
		{
			_delayedWakeup = false;
			qDebug().nospace() << ((isGoingSleep) ? "OS event: going to sleep" : "OS event: waking up");
			emit prepareForSleep(!isGoingSleep, hyperhdr::SystemComponent::SUSPEND);
		}
	}
}

SuspendHandler::SuspendHandler(bool sessionLocker)
{
	if (sessionLocker)
	{
		_sessionMonitor = new SessionMonitorDBus(this);

		if (_sessionMonitor->open()) {
			connect(_sessionMonitor, &SessionMonitorDBus::monitorStateChanged, this, &SuspendHandler::SignalHibernate, Qt::QueuedConnection);
			qDebug() << "THE MONITOR STATE HANDLER IS REGISTERED!";
		}
		else {
			sessionLocker = false;
			_sessionMonitor->deleteLater();
			_sessionMonitor = nullptr;
			qCritical() << "COULD NOT REGISTER MONITOR STATE HANDLER NEEDED BY, FOR EXAMPLE, PIPEWIRE GRABBER (WHICH WONT WORK AS A SERVICE)";
		}
	}

	_systemSuspend = new SystemSuspendDBus(this);

	if (_systemSuspend->open(sessionLocker)) {
		connect(_systemSuspend, &SystemSuspendDBus::prepareForSleep, this, &SuspendHandler::SignalHibernate, Qt::QueuedConnection);
		qDebug() << "THE SLEEP HANDLER IS REGISTERED!";
	}
	else {
		_systemSuspend->deleteLater();
		_systemSuspend = nullptr;
		qCritical() << "COULD NOT REGISTER THE SLEEP HANDLER";
	}
}

SuspendHandler::~SuspendHandler()
{
	qDebug().nospace() << "THE SLEEP HANDLER IS DEREGISTERED!";
}
