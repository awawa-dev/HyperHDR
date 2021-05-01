#include <hyperhdrbase/CaptureCont.h>

#include <hyperhdrbase/HyperHdrInstance.h>

// utils includes
#include <utils/GlobalSignals.h>

// qt includes
#include <QTimer>

CaptureCont::CaptureCont(HyperHdrInstance* hyperhdr)
	: QObject()
	, _hyperhdr(hyperhdr)	
	, _v4lCaptEnabled(false)
	, _v4lCaptPrio(0)
	, _v4lCaptName()
	, _v4lInactiveTimer(new QTimer(this))
{
	// settings changes
	connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &CaptureCont::handleSettingsUpdate);

	// comp changes
	connect(_hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &CaptureCont::handleCompStateChangeRequest);	

	// inactive timer v4l
	connect(_v4lInactiveTimer, &QTimer::timeout, this, &CaptureCont::setV4lInactive);
	_v4lInactiveTimer->setSingleShot(true);
	_v4lInactiveTimer->setInterval(2000);

	// init
	handleSettingsUpdate(settings::type::INSTCAPTURE, _hyperhdr->getSetting(settings::type::INSTCAPTURE));
}

void CaptureCont::handleV4lImage(const QString& name, const Image<ColorRgb> & image)
{
	if(_v4lCaptName != name)
	{
		_hyperhdr->registerInput(_v4lCaptPrio, hyperhdr::COMP_V4L, "System", name);
		_v4lCaptName = name;
	}
	_v4lInactiveTimer->start();
	_hyperhdr->setInputImage(_v4lCaptPrio, image);
}

void CaptureCont::setV4LCaptureEnable(bool enable)
{
	if(_v4lCaptEnabled != enable)
	{
		if(enable)
		{
			_hyperhdr->registerInput(_v4lCaptPrio, hyperhdr::COMP_V4L);
			connect(GlobalSignals::getInstance(), &GlobalSignals::setV4lImage, this, &CaptureCont::handleV4lImage);
			connect(GlobalSignals::getInstance(), &GlobalSignals::setV4lImage, _hyperhdr, &HyperHdrInstance::forwardV4lProtoMessage);
		}
		else
		{
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::setV4lImage, 0, 0);
			_hyperhdr->clear(_v4lCaptPrio);
			_v4lInactiveTimer->stop();
			_v4lCaptName = "";
		}
		_v4lCaptEnabled = enable;
		_hyperhdr->setNewComponentState(hyperhdr::COMP_V4L, enable);
		emit GlobalSignals::getInstance()->requestSource(hyperhdr::COMP_V4L, int(_hyperhdr->getInstanceIndex()), enable);
	}
}

void CaptureCont::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::type::INSTCAPTURE)
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

void CaptureCont::handleCompStateChangeRequest(hyperhdr::Components component, bool enable)
{
	if(component == hyperhdr::COMP_V4L)
	{
		setV4LCaptureEnable(enable);
	}
}

void CaptureCont::setV4lInactive()
{
	_hyperhdr->setInputInactive(_v4lCaptPrio);
}
