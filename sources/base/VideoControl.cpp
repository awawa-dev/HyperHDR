/* VideoControl.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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

#include <base/VideoControl.h>
#include <base/HyperHdrInstance.h>
#include <base/GrabberWrapper.h>
#include <utils/GlobalSignals.h>

#include <QTimer>

bool VideoControl::_stream = false;

VideoControl::VideoControl(HyperHdrInstance* hyperhdr)
	: QObject()
	, _hyperhdr(hyperhdr)
	, _usbCaptEnabled(false)
	, _alive(false)
	, _usbCaptPrio(0)
	, _usbCaptName()
	, _usbInactiveTimer(new QTimer(this))
	, _isCEC(false)
{
	// settings changes
	connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &VideoControl::handleSettingsUpdate);

	// comp changes
	connect(_hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &VideoControl::handleCompStateChangeRequest);

	// inactive timer usb grabber
	connect(&_usbInactiveTimer, &QTimer::timeout, this, &VideoControl::setUsbInactive);

	_usbInactiveTimer.setInterval(800);

	// init
	handleSettingsUpdate(settings::type::VIDEOCONTROL, _hyperhdr->getSetting(settings::type::VIDEOCONTROL));

	connect(this, &VideoControl::setUsbCaptureEnableSignal, this, &VideoControl::setUsbCaptureEnable);
}

bool VideoControl::isCEC()
{
	return _isCEC;
}

void VideoControl::handleUsbImage(const QString& name, const Image<ColorRgb>& image)
{
	_stream = true;

	if (_usbCaptName != name)
	{
		_hyperhdr->registerInput(_usbCaptPrio, hyperhdr::COMP_VIDEOGRABBER, "System", name);
		_usbCaptName = name;
	}

	_alive = true;

	if (!_usbInactiveTimer.isActive() && _usbInactiveTimer.remainingTime() < 0)
		_usbInactiveTimer.start();

	_hyperhdr->setInputImage(_usbCaptPrio, image);
}

void VideoControl::setUsbCaptureEnable(bool enable)
{
	if (_usbCaptEnabled != enable)
	{
		if (enable)
		{
			_hyperhdr->registerInput(_usbCaptPrio, hyperhdr::COMP_VIDEOGRABBER);
			connect(GlobalSignals::getInstance(), &GlobalSignals::setVideoImage, this, &VideoControl::handleUsbImage);
			connect(GlobalSignals::getInstance(), &GlobalSignals::setVideoImage, _hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage);
		}
		else
		{
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::setVideoImage, this, &VideoControl::handleUsbImage);
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::setVideoImage, _hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage);
			_hyperhdr->clear(_usbCaptPrio);
			_usbInactiveTimer.stop();
			_usbCaptName = "";
		}

		_usbCaptEnabled = enable;
		_hyperhdr->setNewComponentState(hyperhdr::COMP_VIDEOGRABBER, enable);
		emit GlobalSignals::getInstance()->requestSource(hyperhdr::COMP_VIDEOGRABBER, int(_hyperhdr->getInstanceIndex()), enable);
	}
}

void VideoControl::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::VIDEOCONTROL)
	{
		const QJsonObject& obj = config.object();
		if (_usbCaptPrio != obj["videoInstancePriority"].toInt(240))
		{
			setUsbCaptureEnable(false); // clear prio
			_usbCaptPrio = obj["videoInstancePriority"].toInt(240);
		}

		setUsbCaptureEnable(obj["videoInstanceEnable"].toBool(true));
		_isCEC = obj["cecControl"].toBool(false);
	}
}

void VideoControl::handleCompStateChangeRequest(hyperhdr::Components component, bool enable)
{
	if (component == hyperhdr::COMP_VIDEOGRABBER)
	{
		setUsbCaptureEnable(enable);
	}
}

void VideoControl::setUsbInactive()
{
	if (!_alive)
	{
		_hyperhdr->setInputInactive(_usbCaptPrio);

		if (_stream)
		{
			_stream = false;
			if (GrabberWrapper::getInstance() != nullptr)
				GrabberWrapper::getInstance()->revive();
		}
	}

	_alive = false;
}
