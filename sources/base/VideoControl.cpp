/* VideoControl.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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

#ifndef PCH_ENABLED
	#include <QTimer>
#endif

#include <base/VideoControl.h>
#include <base/HyperHdrInstance.h>
#include <base/GrabberWrapper.h>
#include <base/GrabberHelper.h>
#include <utils/GlobalSignals.h>

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
	connect(_hyperhdr, &HyperHdrInstance::SignalInstanceSettingsChanged, this, &VideoControl::handleSettingsUpdate);

	// comp changes
	connect(_hyperhdr, &HyperHdrInstance::SignalRequestComponent, this, &VideoControl::handleCompStateChangeRequest);

	// inactive timer usb grabber
	connect(_usbInactiveTimer, &QTimer::timeout, this, &VideoControl::setUsbInactive);

	_usbInactiveTimer->setInterval(800);

	// init
	QJsonDocument settings = _hyperhdr->getSetting(settings::type::VIDEOCONTROL);
	QUEUE_CALL_2(this, handleSettingsUpdate, settings::type, settings::type::VIDEOCONTROL, QJsonDocument, settings);
}

VideoControl::~VideoControl()
{
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::COMP_VIDEOGRABBER, int(_hyperhdr->getInstanceIndex()), false);
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::COMP_CEC, int(_hyperhdr->getInstanceIndex()), false);

	std::cout << "VideoControl exits now" << std::endl;
}

bool VideoControl::isCEC()
{
	return _isCEC;
}

quint8 VideoControl::getCapturePriority()
{
	return _usbCaptPrio;
}

void VideoControl::handleIncomingUsbImage(const QString& name, const Image<ColorRgb>& image)
{
	QMutexLocker locker(&incoming.mutex);
	incoming.frame = image;
	incoming.name = name;
	QUEUE_CALL_0(this, handleUsbImage);
}

void VideoControl::handleUsbImage()
{
	Image<ColorRgb> image;
	QString name;

	if (!_usbCaptEnabled)
		return;
	
	incoming.mutex.lock();
	{
		name = incoming.name;
		image = incoming.frame;
		incoming.frame = Image<ColorRgb>();
	}
	incoming.mutex.unlock();

	if (image.width() <= 1 || image.height() <=1)
		return;

	_stream = true;

	if (_usbCaptName != name)
	{
		_usbCaptName = name;
		_hyperhdr->registerInput(_usbCaptPrio, hyperhdr::COMP_VIDEOGRABBER, "System", _usbCaptName);
	}

	_alive = true;

	if (!_usbInactiveTimer->isActive() && _usbInactiveTimer->remainingTime() < 0)
		_usbInactiveTimer->start();

	_hyperhdr->setInputImage(_usbCaptPrio, image);
}

void VideoControl::setUsbCaptureEnable(bool enable)
{
	if (_usbCaptEnabled != enable)
	{
		if (enable)
		{
			_hyperhdr->registerInput(_usbCaptPrio, hyperhdr::COMP_VIDEOGRABBER, "System", _usbCaptName);
			connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &VideoControl::handleIncomingUsbImage, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
		}
		else
		{
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &VideoControl::handleIncomingUsbImage);
			_hyperhdr->clear(_usbCaptPrio);
			_usbInactiveTimer->stop();
		}

		_usbCaptEnabled = enable;
		_hyperhdr->setNewComponentState(hyperhdr::COMP_VIDEOGRABBER, enable);
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::COMP_VIDEOGRABBER, int(_hyperhdr->getInstanceIndex()), enable);
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
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::COMP_CEC, int(_hyperhdr->getInstanceIndex()), _isCEC);
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
			std::shared_ptr<GrabberHelper> grabberHelper;			
			emit GlobalSignals::getInstance()->SignalGetVideoGrabber(grabberHelper);
			if (grabberHelper != nullptr && grabberHelper->grabberWrapper() != nullptr)
				QUEUE_CALL_0(grabberHelper->grabberWrapper(), revive);
		}
	}

	_alive = false;
}
