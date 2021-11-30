/* smartPipewire.cpp
*
*  MIT License
*
*  Copyright (c) 2022 awawa-dev
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

#include <grabber/smartPipewire.h>
#include <QFlags>
#include <QString>
#include <QThread>

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <cassert>
#include <sys/wait.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <cstring>
#include <iostream>
#include <memory>

#include "PipewireHandler.h"

std::unique_ptr<PipewireHandler> _pipewireHandler(nullptr);


void initPipewireDisplay(const char* restorationToken)
{
	QString qRestorationToken = QString("%1").arg(restorationToken);
	_pipewireHandler = std::unique_ptr<PipewireHandler>(new PipewireHandler());
	_pipewireHandler->startSession(qRestorationToken);
}

void releaseFramePipewire()
{	
	_pipewireHandler->releaseWorkingFrame();
}

const char* getPipewireToken()
{
	static QByteArray tokenData;
	QString token;

	if (_pipewireHandler != nullptr)	
		token = _pipewireHandler->getToken();	

	tokenData = token.toLatin1();

	return tokenData.constData();
}

const char* getPipewireError()
{
	if (_pipewireHandler != nullptr)
	{
		QString err = _pipewireHandler->getError();
		return err.toLatin1().constData();
	}

	return nullptr;
}

void uniniPipewireDisplay()
{	
	if (_pipewireHandler != nullptr)
	{
		_pipewireHandler->closeSession();
		_pipewireHandler = nullptr;
	}
}

bool hasPipewire()
{
	try
	{
		int version = PipewireHandler::readVersion();

		std::cout << "Portal.ScreenCast: protocol version = " << version << std::endl;

		if (version >= 4)
		{
			return true;
		}
		else if (version <= 1 )
		{
			return false;
		}
		else
		{
			QString sessionType = QString("%1").arg(getenv("XDG_SESSION_TYPE")).toLower();
			QString waylandDisplay = QString("%1").arg(getenv("WAYLAND_DISPLAY")).toLower();
			std::cout << "Pipewire: xorg display detection = " << qPrintable(sessionType+" | "+ waylandDisplay) << std::endl;

			if (sessionType.indexOf("wayland") < 0 && waylandDisplay.indexOf("wayland") < 0)
				return false;
			else
				return true;
		}
	}
	catch (...)
	{

	}
	return false;
}

PipewireImage getFramePipewire()
{
	PipewireImage retVal;

	if (_pipewireHandler != nullptr)
	{
		releaseFramePipewire();

		_pipewireHandler->getImage(&retVal);
	}

	return retVal;
}

