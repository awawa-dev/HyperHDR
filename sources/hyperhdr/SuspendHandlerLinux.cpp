/* SuspendHandlerLinux.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2023 awawa-dev
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


#include <QDBusConnection>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <cmath>
#include <cassert>
#include <stdlib.h>
#include "SuspendHandlerLinux.h"
#include <utils/Components.h>
#include <utils/JsonUtils.h>
#include <utils/Image.h>
#include <base/HyperHdrManager.h>
#include <iostream>

namespace {
	const QString UPOWER_SERVICE = QStringLiteral("org.freedesktop.login1");
	const QString UPOWER_PATH = QStringLiteral("/org/freedesktop/login1");
	const QString UPOWER_INTER = QStringLiteral("org.freedesktop.login1.Manager");
}

SuspendHandler::SuspendHandler()
{
	QDBusConnection bus = QDBusConnection::systemBus();

	if (!bus.isConnected())
	{
		std::cout << "SYSTEM BUS IS NOT CONNECTED!" << std::endl;
		return;
	}

	if (!bus.connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_INTER, "PrepareForSleep", this, SLOT(sleeping(bool))))
		std::cout << "COULD NOT REGISTER SLEEP HANDLER!" << std::endl;
	else
		std::cout << "SLEEP HANDLER REGISTERED!" << std::endl;
}

SuspendHandler::~SuspendHandler()
{
	QDBusConnection bus = QDBusConnection::systemBus();

	if (bus.isConnected())
	{
		if (!bus.disconnect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_INTER, "PrepareForSleep", this, SLOT(sleeping(bool))))
			std::cout << "COULD NOT DEREGISTER SLEEP HANDLER!" << std::endl;
		else
			std::cout << "SLEEP HANDLER DEREGISTERED!" << std::endl;
	}
}

void SuspendHandler::sleeping(bool sleep)
{	
	if (sleep)
	{
		std::cout << "OS event: going sleep" << std::endl;
		emit SignalHibernate(false);
	}
	else
	{
		std::cout << "OS event: wake up" << std::endl;
		emit SignalHibernate(true);
	}
}

