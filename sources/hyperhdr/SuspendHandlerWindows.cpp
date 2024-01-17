/* SuspendHandlerWindows.cpp
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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <cmath>
#include <cassert>
#include <stdlib.h>
#include "SuspendHandlerWindows.h"
#include <utils/Components.h>
#include <utils/JsonUtils.h>
#include <utils/Image.h>
#include <base/HyperHdrManager.h>
#include <windows.h>
#include <wtsapi32.h>

#pragma comment (lib, "WtsApi32.Lib")

SuspendHandler::SuspendHandler(bool sessionLocker)
{
	auto handle = reinterpret_cast<HWND> (_widget.winId());
	_notifyHandle = RegisterSuspendResumeNotification(handle, DEVICE_NOTIFY_WINDOW_HANDLE);

	if (_notifyHandle == NULL)
		std::cout << "COULD NOT REGISTER SLEEP HANDLER!" << std::endl;
	else
		std::cout << "Sleep handler registered!" << std::endl;

	_sessionLocker = sessionLocker;
	if (_sessionLocker)
	{
		if (WTSRegisterSessionNotification(handle, NOTIFY_FOR_THIS_SESSION))
			std::cout << "Session handler registered!" << std::endl;
		else
		{
			std::cout << "COULD NOT REGISTER SESSION HANDLER!" << std::endl;
			_sessionLocker = false;
		}
	}
}

SuspendHandler::~SuspendHandler()
{
	if (_notifyHandle != NULL)
	{
		UnregisterSuspendResumeNotification(_notifyHandle);
		std::cout << "Sleep handler deregistered!" << std::endl;
	}
	_notifyHandle = NULL;

	if (_sessionLocker)
	{
		auto handle = reinterpret_cast<HWND> (_widget.winId());
		WTSUnRegisterSessionNotification(handle);
	}
}

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
bool SuspendHandler::nativeEventFilter(const QByteArray& eventType, void* message, long* result)
#else
bool SuspendHandler::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
#endif
{
	MSG* msg = static_cast<MSG*>(message);

	if (msg->message == WM_POWERBROADCAST)
	{
		switch (msg->wParam)
		{			
			case PBT_APMRESUMESUSPEND:
				emit SignalHibernate(true);
				return true;
				break;
			case PBT_APMSUSPEND:
				emit SignalHibernate(false);
				return true;
				break;
		}
	}

	if (_sessionLocker)
	{
		if (msg->message == WM_WTSSESSION_CHANGE)
		{
			switch (msg->wParam)
			{
			case WTS_SESSION_UNLOCK:
				emit SignalHibernate(true);
				return true;
				break;

			case WTS_SESSION_LOCK:

				if (GetSystemMetrics(SM_REMOTESESSION) != 0)
				{
					std::cout << "Detected RDP session. Skipping disable on lock." << std::endl;
				}
				else
				{
					emit SignalHibernate(false);
					return true;
				}
				break;
			}
		}
	}

	return false;
}

