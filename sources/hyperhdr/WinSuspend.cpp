/* WinSuspend.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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
#include "WinSuspend.h"
#include <utils/Components.h>
#include <utils/JsonUtils.h>
#include <utils/Image.h>
#include <base/HyperHdrIManager.h>
#include <windows.h>

SuspendHandler::SuspendHandler()
{
	auto handle = reinterpret_cast<HWND> (widget.winId());
	RegisterSuspendResumeNotification(handle, DEVICE_NOTIFY_WINDOW_HANDLE);
}

bool SuspendHandler::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
	MSG* msg = static_cast<MSG*>(message);

	if (msg->message == WM_POWERBROADCAST)
	{
		switch (msg->wParam)
		{			
			case PBT_APMRESUMESUSPEND:
				QMetaObject::invokeMethod(HyperHdrIManager::getInstance(), "hibernate", Qt::QueuedConnection, Q_ARG(bool, true));
				return true;
				break;
			case PBT_APMSUSPEND:
				QMetaObject::invokeMethod(HyperHdrIManager::getInstance(), "hibernate", Qt::QueuedConnection, Q_ARG(bool, false));
				return true;
				break;
		}
	}
	return false;
}

