#include <hyperion/CaptureCont.h>

// hyperion includes
#include <hyperion/Hyperion.h>

// utils includes
#include <utils/GlobalSignals.h>

// qt includes
#include <QTimer>

CaptureCont::CaptureCont(Hyperion* hyperion)
	: QObject()
	, _hyperion(hyperion)	
	, _v4lCaptEnabled(false)
	, _v4lCaptPrio(0)
	, _v4lCaptName()
	, _v4lInactiveTimer(new QTimer(this))
{
	// settings changes
	connect(_hyperion, &Hyperion::settingsChanged, this, &CaptureCont::handleSettingsUpdate);

	// comp changes
	connect(_hyperion, &Hyperion::compStateChangeRequest, this, &CaptureCont::handleCompStateChangeRequest);	

	// inactive timer v4l
	connect(_v4lInactiveTimer, &QTimer::timeout, this, &CaptureCont::setV4lInactive);
	_v4lInactiveTimer->setSingleShot(true);
	_v4lInactiveTimer->setInterval(1000);

	// init
	handleSettingsUpdate(settings::INSTCAPTURE, _hyperion->getSetting(settings::INSTCAPTURE));
}

void CaptureCont::handleV4lImage(const QString& name, const Image<ColorRgb> & image)
{
	if(_v4lCaptName != name)
	{
		_hyperion->registerInput(_v4lCaptPrio, hyperion::COMP_V4L, "System", name);
		_v4lCaptName = name;
	}
	_v4lInactiveTimer->start();
	_hyperion->setInputImage(_v4lCaptPrio, image);
}

void CaptureCont::setV4LCaptureEnable(bool enable)
{
	if(_v4lCaptEnabled != enable)
	{
		if(enable)
		{
			_hyperion->registerInput(_v4lCaptPrio, hyperion::COMP_V4L);
			connect(GlobalSignals::getInstance(), &GlobalSignals::setV4lImage, this, &CaptureCont::handleV4lImage);
			connect(GlobalSignals::getInstance(), &GlobalSignals::setV4lImage, _hyperion, &Hyperion::forwardV4lProtoMessage);
		}
		else
		{
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::setV4lImage, 0, 0);
			_hyperion->clear(_v4lCaptPrio);
			_v4lInactiveTimer->stop();
			_v4lCaptName = "";
		}
		_v4lCaptEnabled = enable;
		_hyperion->setNewComponentState(hyperion::COMP_V4L, enable);
		emit GlobalSignals::getInstance()->requestSource(hyperion::COMP_V4L, int(_hyperion->getInstanceIndex()), enable);
	}
}

void CaptureCont::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::INSTCAPTURE)
	{
		const QJsonObject& obj = config.object();
		if(_v4lCaptPrio != obj["v4lPriority"].toInt(240))
		{
			setV4LCaptureEnable(false); // clear prio
			_v4lCaptPrio = obj["v4lPriority"].toInt(240);
		}

		setV4LCaptureEnable(obj["v4lEnable"].toBool(true));
	}
}

void CaptureCont::handleCompStateChangeRequest(hyperion::Components component, bool enable)
{
	if(component == hyperion::COMP_V4L)
	{
		setV4LCaptureEnable(enable);
	}
}

void CaptureCont::setV4lInactive()
{
	_hyperion->setInputInactive(_v4lCaptPrio);
}
