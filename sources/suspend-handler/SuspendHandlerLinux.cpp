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


#include <sdbus-c++/sdbus-c++.h>
#include <algorithm>
#include <base/HyperHdrManager.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <image/Image.h>
#include <iostream>
#include <limits>
#include <suspend-handler/SuspendHandlerLinux.h>
#include <utils/Components.h>

using namespace sdbus;

namespace {
	constexpr const char* UPOWER_SERVICE = "org.freedesktop.login1";
	constexpr const char* UPOWER_PATH = "/org/freedesktop/login1";
	constexpr const char* UPOWER_INTERFACE = "org.freedesktop.login1.Manager";
	constexpr const char* GNOME_SERVICE = "org.gnome.ScreenSaver";
	constexpr const char* GNOME_PATH = "/org/gnome/ScreenSaver";
	constexpr const char* KDE_SERVICE = "org.freedesktop.ScreenSaver";
	constexpr const char* KDE_PATH = "/org/freedesktop/ScreenSaver";
	constexpr const char* XFCE_SERVICE = "org.xfce.PowerManager";
	constexpr const char* XFCE_PATH = "/org/xfce/PowerManager";
}

SuspendHandler::SuspendHandler(bool sessionLocker)
{
	if (sessionLocker)
	{
		try
		{
			auto monitorSignalHandler = [this](QString source, bool isOff) {
				qDebug().nospace() << "Display event: monitor is " << (isOff ? "OFF" : "ON") << " (reporting: " << source << ")";
				emit SignalHibernate(!isOff, hyperhdr::SystemComponent::MONITOR);
				};

			_sessionBus = sdbus::createSessionBusConnection();
			_gnomeScreenSaverProxy = sdbus::createProxy(*_sessionBus, ServiceName{ GNOME_SERVICE }, ObjectPath{ GNOME_PATH });
			_gnomeScreenSaverProxy->uponSignal(SignalName{ "ActiveChanged" }).onInterface(InterfaceName{ GNOME_SERVICE }).call([monitorSignalHandler](bool active) {
				monitorSignalHandler(QString(GNOME_SERVICE), active);
				});
			_kdePowerProxy = sdbus::createProxy(*_sessionBus, ServiceName{ KDE_SERVICE }, ObjectPath{ KDE_PATH });
			_kdePowerProxy->uponSignal(SignalName{ "ActiveChanged" }).onInterface(InterfaceName{ KDE_SERVICE }).call([monitorSignalHandler](bool active) {
				monitorSignalHandler(QString(KDE_SERVICE), active);
				});
			_xfcePowerProxy = sdbus::createProxy(*_sessionBus, ServiceName{ XFCE_SERVICE }, ObjectPath{ XFCE_PATH });
			_xfcePowerProxy->uponSignal(SignalName{ "StateChanged" }).onInterface(InterfaceName{ XFCE_SERVICE }).call([monitorSignalHandler](uint32_t state) {
				monitorSignalHandler(QString(XFCE_SERVICE), state == 3);
				});
			_sessionBus->enterEventLoopAsync();

			qDebug().nospace() << "THE MONITOR STATE HANDLER IS REGISTERED!";
		}
		catch (std::exception& ex)
		{
			qCritical().nospace() << "COULD NOT REGISTER MONITOR STATE HANDLER NEEDED BY, FOR EXAMPLE, PIPEWIRE GRABBER (WHICH WONT WORK AS A SERVICE): " << ex.what();
		}
	}

	try
	{
		auto responseSignalHandler = [this](bool sleep) {
			QUEUE_CALL_1(this, sleeping, bool, sleep);
		};

		_dbusConnection = sdbus::createSystemBusConnection();
		_suspendHandlerProxy = sdbus::createProxy(*_dbusConnection, ServiceName{ UPOWER_SERVICE }, ObjectPath{ UPOWER_PATH });
		_suspendHandlerProxy->uponSignal(SignalName{ "PrepareForSleep" }).onInterface(InterfaceName{ UPOWER_INTERFACE }).call(responseSignalHandler);
		_dbusConnection->enterEventLoopAsync();

		qDebug().nospace() << "THE SLEEP HANDLER IS REGISTERED!";
	}
	catch (std::exception& ex)
	{
		qCritical().nospace() << "COULD NOT REGISTER THE SLEEP HANDLER: " << ex.what();
	}
}

SuspendHandler::~SuspendHandler()
{
	try
	{
		_xfcePowerProxy = nullptr;
		_gnomeScreenSaverProxy = nullptr;
		_kdePowerProxy = nullptr;
		_sessionBus = nullptr;

		_suspendHandlerProxy = nullptr;
		_dbusConnection = nullptr;

		qDebug().nospace() << "THE SLEEP HANDLER IS DEREGISTERED!";
	}
	catch (std::exception& ex)
	{
		qCritical().nospace() << "COULD NOT DEREGISTER THE SLEEP HANDLER: " << ex.what();
	}
}

void SuspendHandler::sleeping(bool sleep)
{	
	if (sleep)
	{
		qDebug().nospace() << "OS event: going to sleep";
		emit SignalHibernate(false, hyperhdr::SystemComponent::SUSPEND);
	}
	else
	{
		qDebug().nospace() << "OS event: waking up";
		emit SignalHibernate(true, hyperhdr::SystemComponent::SUSPEND);
	}
}

