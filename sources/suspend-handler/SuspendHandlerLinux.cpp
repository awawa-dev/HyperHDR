/* SuspendHandlerLinux.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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
#include <cstdint>
#include <limits>
#include <cmath>
#include <cassert>
#include <stdlib.h>
#include <sdbus-c++/sdbus-c++.h>
#include <suspend-handler/SuspendHandlerLinux.h>
#include <utils/Components.h>
#include <image/Image.h>
#include <base/HyperHdrManager.h>
#include <iostream>

using namespace sdbus;

namespace {
	const QString UPOWER_SERVICE = QStringLiteral("org.freedesktop.login1");
	const QString UPOWER_PATH = QStringLiteral("/org/freedesktop/login1");
	const QString UPOWER_INTERFACE = QStringLiteral("org.freedesktop.login1.Manager");
}

SuspendHandler::SuspendHandler(bool sessionLocker)
{
	try
	{
		auto responseSignalHandler = [&](bool sleep) {				
			QUEUE_CALL_1(this, sleeping, bool, sleep);
		};

		_dbusConnection = sdbus::createSystemBusConnection();
		_suspendHandlerProxy = sdbus::createProxy(*_dbusConnection, ServiceName{ UPOWER_SERVICE.toStdString() }, ObjectPath{ UPOWER_PATH.toStdString() });
		_suspendHandlerProxy->uponSignal(SignalName{ "PrepareForSleep" }).onInterface(InterfaceName{ UPOWER_INTERFACE.toStdString() }).call(responseSignalHandler);
		_dbusConnection->enterEventLoopAsync();

		_suspendHandlerProxy = nullptr;
		_dbusConnection = nullptr;
		std::cout << "THE SLEEP HANDLER IS REGISTERED!" << std::endl;
	}
	catch (std::exception& ex)
	{
		std::cout << "COULD NOT REGISTER THE SLEEP HANDLER: " << ex.what() << std::endl;
	}
}

SuspendHandler::~SuspendHandler()
{
	try
	{
		_suspendHandlerProxy = nullptr;
		_dbusConnection = nullptr;
		std::cout << "THE SLEEP HANDLER IS DEREGISTERED!" << std::endl;
	}
	catch (std::exception& ex)
	{
		std::cout << "COULD NOT DEREGISTER THE SLEEP HANDLER: " << ex.what() << std::endl;
	}
}

void SuspendHandler::sleeping(bool sleep)
{	
	if (sleep)
	{
		std::cout << "OS event: going to sleep" << std::endl;
		emit SignalHibernate(false, hyperhdr::SystemComponent::SUSPEND);
	}
	else
	{
		std::cout << "OS event: waking up" << std::endl;
		emit SignalHibernate(true, hyperhdr::SystemComponent::SUSPEND);
	}
}

