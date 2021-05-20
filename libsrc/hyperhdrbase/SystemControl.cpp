#include <hyperhdrbase/SystemControl.h>

#include <hyperhdrbase/HyperHdrInstance.h>

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
	_sysInactiveTimer.setInterval(2000);

	// init
	handleSettingsUpdate(settings::type::SYSTEMCONTROL, _hyperhdr->getSetting(settings::type::SYSTEMCONTROL));

	connect(this, &SystemControl::setSysCaptureEnableSignal, this, &SystemControl::setSysCaptureEnable);
}

bool SystemControl::isCEC()
{
	return _isCEC;
}

void SystemControl::handleSysImage(const QString& name, const Image<ColorRgb> & image)
{
	if(_sysCaptName != name)
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
	if(_sysCaptEnabled != enable)
	{
		if(enable)
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
	if(type == settings::type::SYSTEMCONTROL)
	{
		const QJsonObject& obj = config.object();
		if(_sysCaptPrio != obj["systemInstancePriority"].toInt(245))
		{
			setSysCaptureEnable(false); // clear prio
			_sysCaptPrio = obj["systemInstancePriority"].toInt(245);
		}

		setSysCaptureEnable(obj["systemInstanceEnable"].toBool(false));
		_isCEC = obj["cecControl"].toBool(false);
	}
}

void SystemControl::handleCompStateChangeRequest(hyperhdr::Components component, bool enable)
{
	if(component == hyperhdr::COMP_SYSTEMGRABBER)
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
