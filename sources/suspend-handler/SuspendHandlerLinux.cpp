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
#include <base/HyperHdrManager.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <image/Image.h>
#include <iostream>
#include <limits>
#include <QLatin1String>
#include <suspend-handler/SuspendHandlerLinux.h>
#include <suspend-handler/SuspendHandlerLinuxDBus.h>
#include <utils/Components.h>

namespace
{
	constexpr QLatin1String GnomeService{ "org.gnome.ScreenSaver" };
	constexpr QLatin1String GnomePath{ "/org/gnome/ScreenSaver" };
	constexpr QLatin1String KdeService{ "org.freedesktop.ScreenSaver" };
	constexpr QLatin1String KdePath{ "/org/freedesktop/ScreenSaver" };
	constexpr QLatin1String XfceService{ "org.xfce.ScreenSaver" };
	constexpr QLatin1String XfcePath{ "/org/xfce/ScreenSaver" };
	constexpr QLatin1String Login1Service{ "org.freedesktop.login1" };
	constexpr QLatin1String Login1Path{ "/org/freedesktop/login1" };
	constexpr QLatin1String Login1Interface{ "org.freedesktop.login1.Manager" };
}

SessionMonitorDBus::SessionMonitorDBus(QObject* parent) : HelperDBus(parent) {
	connect(this, &HelperDBus::signalReceived, this, &SessionMonitorDBus::handleSignal);
};

bool SessionMonitorDBus::open() {
	if (!HelperDBus::open(DBUS_BUS_SESSION))
		return false;

	if (!(addSignalMatch(GnomeService, GnomePath, GnomeService, "ActiveChanged") | addSignalMatch(KdeService, KdePath, KdeService, "ActiveChanged") | addSignalMatch(XfceService, XfcePath, XfceService, "ActiveChanged")))
	{
		closeConnection();
		return false;
	}

	return true;
};

void SessionMonitorDBus::handleSignal(const QString& , const QString& , const QString& , const QVariantList& arguments, bool parseError) {
	if (parseError || arguments.size() < 1 || !HelperDBus::isType<bool>(arguments[0]))
		return;
	
	emit monitorStateChanged("ScreenSaver", arguments[0].toBool());	
}

SystemSuspendDBus::SystemSuspendDBus(QObject* parent) : HelperDBus(parent) {
	connect(this, &HelperDBus::signalReceived, this, &SystemSuspendDBus::handleSignal);
}

bool SystemSuspendDBus::open() {
	if (!HelperDBus::open(DBUS_BUS_SYSTEM))
		return false;

	if (!addSignalMatch(Login1Service, Login1Path, Login1Interface, "PrepareForSleep")) {
		closeConnection();
		return false;
	}

	return true;
}

void SystemSuspendDBus::handleSignal(const QString& path, const QString& interface, const QString& member, const QVariantList& arguments, bool parseError) {
	if (parseError || arguments.size() < 1 || !HelperDBus::isType<bool>(arguments[0]))
		return;

	emit prepareForSleep(arguments[0].toBool());
}

SuspendHandler::SuspendHandler(bool sessionLocker)
{
	if (sessionLocker)
	{
		_sessionMonitor = new SessionMonitorDBus(this);

		connect(_sessionMonitor, &SessionMonitorDBus::monitorStateChanged, this, [this](const QString& source, bool monitorOff) {
			qDebug().nospace() << "Display event: monitor is " << (monitorOff ? "OFF" : "ON") << " (reporting: " << source << ")";
			emit SignalHibernate(!monitorOff, hyperhdr::SystemComponent::MONITOR);
		}, Qt::QueuedConnection);

		if (_sessionMonitor->open())
			qDebug() << "THE MONITOR STATE HANDLER IS REGISTERED!";
		else
			qCritical() << "COULD NOT REGISTER MONITOR STATE HANDLER NEEDED BY, FOR EXAMPLE, PIPEWIRE GRABBER (WHICH WONT WORK AS A SERVICE)";
	}

	_systemSuspend = new SystemSuspendDBus(this);

	connect(_systemSuspend, &SystemSuspendDBus::prepareForSleep, this, [this](bool sleep){
		qDebug().nospace() << ((sleep) ? "OS event: going to sleep" : "OS event: waking up");
		emit SignalHibernate(!sleep, hyperhdr::SystemComponent::SUSPEND);
	}, Qt::QueuedConnection);

	if (_systemSuspend->open())
		qDebug() << "THE SLEEP HANDLER IS REGISTERED!";
	else
		qCritical() << "COULD NOT REGISTER THE SLEEP HANDLER";
}

SuspendHandler::~SuspendHandler()
{	
	qDebug().nospace() << "THE SLEEP HANDLER IS DEREGISTERED!";
}
