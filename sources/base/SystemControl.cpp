/* SystemControl.cpp
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

#include <base/SystemControl.h>
#include <base/SystemWrapper.h>
#include <base/HyperHdrInstance.h>

// utils includes
#include <utils/GlobalSignals.h>

// qt includes
#include <QTimer>

SystemControl::SystemControl(HyperHdrInstance* hyperhdr)
	: QObject()
	, _hyperhdr(hyperhdr)
	, _sysCaptEnabled(false)
	, _alive(false)
	, _sysCaptPrio(0)
	, _sysCaptName()
	, _sysInactiveTimer(new QTimer(this))
	, _isCEC(false)
{
	// settings changes
	connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &SystemControl::handleSettingsUpdate);

	// comp changes
	connect(_hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &SystemControl::handleCompStateChangeRequest);

	// inactive timer system grabber
	connect(&_sysInactiveTimer, &QTimer::timeout, this, &SystemControl::setSysInactive);
	_sysInactiveTimer.setInterval(800);

	// init
	handleSettingsUpdate(settings::type::SYSTEMCONTROL, _hyperhdr->getSetting(settings::type::SYSTEMCONTROL));

	connect(this, &SystemControl::setSysCaptureEnableSignal, this, &SystemControl::setSysCaptureEnable);
}

bool SystemControl::isCEC()
{
	return _isCEC;
}

void SystemControl::handleSysImage(const QString& name, const Image<ColorRgb>& image)
{
	if (_sysCaptName != name)
	{
		_hyperhdr->registerInput(_sysCaptPrio, hyperhdr::COMP_SYSTEMGRABBER, "System", name);
		_sysCaptName = name;
	}

	_alive = true;

	if (!_sysInactiveTimer.isActive() && _sysInactiveTimer.remainingTime() < 0)
		_sysInactiveTimer.start();

	_hyperhdr->setInputImage(_sysCaptPrio, image);
}

void SystemControl::setSysCaptureEnable(bool enable)
{
	if (_sysCaptEnabled != enable)
	{
		if (enable)
		{
			_hyperhdr->registerInput(_sysCaptPrio, hyperhdr::COMP_SYSTEMGRABBER);
			connect(GlobalSignals::getInstance(), &GlobalSignals::setSystemImage, this, &SystemControl::handleSysImage);
			connect(GlobalSignals::getInstance(), &GlobalSignals::setSystemImage, _hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage);
		}
		else
		{
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::setSystemImage, this, &SystemControl::handleSysImage);
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::setSystemImage, _hyperhdr, &HyperHdrInstance::forwardSystemProtoMessage);
			_hyperhdr->clear(_sysCaptPrio);
			_sysInactiveTimer.stop();
			_sysCaptName = "";
		}

		_sysCaptEnabled = enable;
		_hyperhdr->setNewComponentState(hyperhdr::COMP_SYSTEMGRABBER, enable);
		emit GlobalSignals::getInstance()->requestSource(hyperhdr::COMP_SYSTEMGRABBER, int(_hyperhdr->getInstanceIndex()), enable);
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

		bool enabledCurrent = obj["systemInstanceEnable"].toBool(false);
		setSysCaptureEnable(enabledCurrent);
		_isCEC = obj["cecControl"].toBool(false);

		if (!enabledCurrent && SystemWrapper::getInstance() != nullptr)
			SystemWrapper::getInstance()->stateChanged(false);
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
