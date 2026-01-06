/* SystemControl.cpp
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

#ifndef PCH_ENABLED
	#include <QTimer>
#endif

#include <base/SystemControl.h>
#include <base/SystemWrapper.h>
#include <base/HyperHdrInstance.h>
#include <utils/GlobalSignals.h>

SystemControl::SystemControl(HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr)
	, _hyperhdr(hyperhdr)
	, _sysCaptEnabled(false)
	, _alive(false)
	, _sysCaptPrio(0)
	, _sysCaptName()
	, _sysInactiveTimer(new QTimer(this))
	, _isCEC(false)
{
	// settings changes
	connect(_hyperhdr, &HyperHdrInstance::SignalInstanceSettingsChanged, this, &SystemControl::handleSettingsUpdate);

	// comp changes
	connect(_hyperhdr, &HyperHdrInstance::SignalRequestComponent, this, &SystemControl::handleCompStateChangeRequest);

	// inactive timer system grabber
	connect(_sysInactiveTimer, &QTimer::timeout, this, &SystemControl::setSysInactive);
	_sysInactiveTimer->setInterval(800);

	// init
	QJsonDocument settings = _hyperhdr->getSetting(settings::type::SYSTEMCONTROL);
	QUEUE_CALL_2(this, handleSettingsUpdate, settings::type, settings::type::SYSTEMCONTROL, QJsonDocument, settings);
}

SystemControl::~SystemControl()
{
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::COMP_SYSTEMGRABBER, int(_hyperhdr->getInstanceIndex()), false);

	std::cout << "SystemControl exits now" << std::endl;
}

quint8 SystemControl::getCapturePriority()
{
	return _sysCaptPrio;
}

bool SystemControl::isCEC()
{
	return _isCEC;
}

void SystemControl::handleSysImage(const QString& name, const Image<ColorRgb>& image)
{
	if (!_sysCaptEnabled)
		return;

	if (_sysCaptName != name)
	{
		_sysCaptName = name;
		_hyperhdr->registerInput(_sysCaptPrio, hyperhdr::COMP_SYSTEMGRABBER, "System", _sysCaptName);
	}

	_alive = true;

	if (!_sysInactiveTimer->isActive() && _sysInactiveTimer->remainingTime() < 0)
		_sysInactiveTimer->start();

	_hyperhdr->setInputImage(_sysCaptPrio, image);
}

void SystemControl::setSysCaptureEnable(bool enable)
{
	if (_sysCaptEnabled != enable)
	{
		if (enable)
		{
			_hyperhdr->registerInput(_sysCaptPrio, hyperhdr::COMP_SYSTEMGRABBER, "System", _sysCaptName);
			connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &SystemControl::handleSysImage, Qt::UniqueConnection);
		}
		else
		{
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &SystemControl::handleSysImage);
			_hyperhdr->clear(_sysCaptPrio);
			_sysInactiveTimer->stop();
		}

		_sysCaptEnabled = enable;
		_hyperhdr->setNewComponentState(hyperhdr::COMP_SYSTEMGRABBER, enable);
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::COMP_SYSTEMGRABBER, int(_hyperhdr->getInstanceIndex()), enable);
	}
}

void SystemControl::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::SYSTEMCONTROL)
	{
		const QJsonObject& obj = config.object();
		if (_sysCaptPrio != obj["systemInstancePriority"].toInt(245))
		{
			setSysCaptureEnable(false); // clear prio
			_sysCaptPrio = obj["systemInstancePriority"].toInt(245);
		}

		setSysCaptureEnable(obj["systemInstanceEnable"].toBool(false));
		_isCEC = obj["cecControl"].toBool(false);
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::COMP_CEC, -int(_hyperhdr->getInstanceIndex()) - 2, _isCEC);
	}
}

void SystemControl::handleCompStateChangeRequest(hyperhdr::Components component, bool enable)
{
	if (component == hyperhdr::COMP_SYSTEMGRABBER)
	{
		setSysCaptureEnable(enable);
	}
}

void SystemControl::setSysInactive()
{
	if (!_alive)
		_hyperhdr->setInputInactive(_sysCaptPrio);

	_alive = false;
}
