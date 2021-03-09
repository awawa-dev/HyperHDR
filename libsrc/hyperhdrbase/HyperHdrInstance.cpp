// STL includes
#include <exception>
#include <sstream>

// QT includes
#include <QString>
#include <QStringList>
#include <QThread>

#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/MessageForwarder.h>
#include <hyperhdrbase/ImageProcessor.h>
#include <hyperhdrbase/ColorAdjustment.h>

// utils
#include <utils/hyperhdr.h>
#include <utils/GlobalSignals.h>
#include <utils/Logger.h>

// LedDevice includes
#include <leddevice/LedDeviceWrapper.h>

#include <hyperhdrbase/MultiColorAdjustment.h>
#include <hyperhdrbase/LinearColorSmoothing.h>

// effect engine includes
#include <effectengine/EffectEngine.h>

// settingsManagaer
#include <hyperhdrbase/SettingsManager.h>

// BGEffectHandler
#include <hyperhdrbase/BGEffectHandler.h>

// CaptureControl (Daemon capture)
#include <hyperhdrbase/CaptureCont.h>

// Boblight
#include <boblightserver/BoblightServer.h>

HyperHdrInstance::HyperHdrInstance(quint8 instance, bool readonlyMode)
	: QObject()
	, _instIndex(instance)
	, _bootEffect(QTime::currentTime().addSecs(5))
	, _settingsManager(new SettingsManager(instance, this, readonlyMode))
	, _componentRegister(this)
	, _ledString(hyperhdr::createLedString(getSetting(settings::type::LEDS).array(), hyperhdr::createColorOrder(getSetting(settings::type::DEVICE).object())))
	, _imageProcessor(new ImageProcessor(_ledString, this))
	, _muxer(static_cast<int>(_ledString.leds().size()), this)
	, _raw2ledAdjustment(hyperhdr::createLedColorsAdjustment(static_cast<int>(_ledString.leds().size()), getSetting(settings::type::COLOR).object()))
	, _ledDeviceWrapper(nullptr)
	, _deviceSmooth(nullptr)
	, _effectEngine(nullptr)
	, _messageForwarder(nullptr)
	, _log(Logger::getInstance("HYPERHDR"))
	, _hwLedCount()
	, _ledGridSize(hyperhdr::getLedLayoutGridSize(getSetting(settings::type::LEDS).array()))
	, _BGEffectHandler(nullptr)
	,_captureCont(nullptr)
	, _globalLedBuffer(_ledString.leds().size(), ColorRgb::BLACK)
	, _boblightServer(nullptr)
	, _readOnlyMode(readonlyMode)
	
{

}

HyperHdrInstance::~HyperHdrInstance()
{
	freeObjects();
}

void HyperHdrInstance::start()
{
	connect(_settingsManager, &SettingsManager::settingsChanged, this, &HyperHdrInstance::settingsChanged);

	connect(this, &HyperHdrInstance::newVideoModeHdr, this, &HyperHdrInstance::handleNewVideoModeHdr);

	if (!_raw2ledAdjustment->verifyAdjustments())
	{
		Warning(_log, "At least one led has no color calibration, please add all leds from your led layout to an 'LED index' field!");
	}

	// handle hwLedCount
	_hwLedCount = qMax(getSetting(settings::type::DEVICE).object()["hardwareLedCount"].toInt(getLedCount()), getLedCount());

	// Initialize colororder vector
	for (const Led& led : _ledString.leds())
	{
		_ledStringColorOrder.push_back(led.colorOrder);
	}


	connect(&_muxer, &PriorityMuxer::visiblePriorityChanged, this, &HyperHdrInstance::update);
	connect(&_muxer, &PriorityMuxer::visiblePriorityChanged, this, &HyperHdrInstance::handlePriorityChangedLedDevice);
	connect(&_muxer, &PriorityMuxer::visibleComponentChanged, this, &HyperHdrInstance::handleVisibleComponentChanged);

	// listens for ComponentRegister changes of COMP_ALL to perform core enable/disable actions


	// listen for settings updates of this instance (LEDS & COLOR)
	connect(_settingsManager, &SettingsManager::settingsChanged, this, &HyperHdrInstance::handleSettingsUpdate);

	#if 0
	// set color correction activity state
	const QJsonObject color = getSetting(settings::COLOR).object();
	#endif

	// initialize LED-devices
	QJsonObject ledDevice = getSetting(settings::type::DEVICE).object();
	ledDevice["currentLedCount"] = _hwLedCount; // Inject led count info

	_ledDeviceWrapper = new LedDeviceWrapper(this);
	connect(this, &HyperHdrInstance::compStateChangeRequest, _ledDeviceWrapper, &LedDeviceWrapper::handleComponentState);
	connect(this, &HyperHdrInstance::ledDeviceData, _ledDeviceWrapper, &LedDeviceWrapper::updateLeds);
	_ledDeviceWrapper->createLedDevice(ledDevice);

	// smoothing
	_deviceSmooth = new LinearColorSmoothing(getSetting(settings::type::SMOOTHING), this);
	connect(this, &HyperHdrInstance::settingsChanged, _deviceSmooth, &LinearColorSmoothing::handleSettingsUpdate);

	// create the message forwarder only on main instance
	if (_instIndex == 0)
	{
		_messageForwarder = new MessageForwarder(this);
	}

	// create the effect engine; needs to be initialized after smoothing!
	_effectEngine = new EffectEngine(this);
	connect(_effectEngine, &EffectEngine::effectListUpdated, this, &HyperHdrInstance::effectListUpdated);

	

	// handle background effect
	_BGEffectHandler = new BGEffectHandler(this);

	// create the Daemon capture interface
	_captureCont = new CaptureCont(this);

	// forwards global signals to the corresponding slots
	connect(GlobalSignals::getInstance(), &GlobalSignals::registerGlobalInput, this, &HyperHdrInstance::registerInput);
	connect(GlobalSignals::getInstance(), &GlobalSignals::clearGlobalInput, this, &HyperHdrInstance::clear);
	connect(GlobalSignals::getInstance(), &GlobalSignals::setGlobalColor, this, &HyperHdrInstance::setColor);
	connect(GlobalSignals::getInstance(), &GlobalSignals::setGlobalImage, this, &HyperHdrInstance::setInputImage);

	// if there is no startup / background effect and no sending capture interface we probably want to push once BLACK (as PrioMuxer won't emit a priority change)
	update();

	// boblight, can't live in global scope as it depends on layout
	_boblightServer = new BoblightServer(this, getSetting(settings::type::BOBLSERVER));
	connect(this, &HyperHdrInstance::settingsChanged, _boblightServer, &BoblightServer::handleSettingsUpdate);

	// instance initiated, enter thread event loop
	emit started();
}

void HyperHdrInstance::stop()
{
	emit finished();
	thread()->wait();
}

void HyperHdrInstance::freeObjects()
{
	// switch off all leds
	clear(-1,true);

	// delete components on exit
	delete _boblightServer;
	delete _captureCont;
	delete _effectEngine;
	delete _raw2ledAdjustment;
	delete _messageForwarder;
	delete _settingsManager;
	delete _ledDeviceWrapper;
}

void HyperHdrInstance::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{

//	std::cout << config.toJson().toStdString() << std::endl;

	if(type == settings::type::COLOR)
	{
		const QJsonObject obj = config.object();
		// change in color recreate ledAdjustments
		delete _raw2ledAdjustment;
		_raw2ledAdjustment = hyperhdr::createLedColorsAdjustment(static_cast<int>(_ledString.leds().size()), obj);

		if (!_raw2ledAdjustment->verifyAdjustments())
		{
			Warning(_log, "At least one led has no color calibration, please add all leds from your led layout to an 'LED index' field!");
		}
	}
	else if(type == settings::type::LEDS)
	{
		const QJsonArray leds = config.array();

		// stop and cache all running effects, as effects depend heavily on LED-layout
		_effectEngine->cacheRunningEffects();

		// ledstring, img processor, muxer, ledGridSize (effect-engine image based effects), _ledBuffer and ByteOrder of ledstring
		_ledString = hyperhdr::createLedString(leds, hyperhdr::createColorOrder(getSetting(settings::type::DEVICE).object()));
		_imageProcessor->setLedString(_ledString);
		_muxer.updateLedColorsLength(static_cast<int>(_ledString.leds().size()));
		_ledGridSize = hyperhdr::getLedLayoutGridSize(leds);

		std::vector<ColorRgb> color(_ledString.leds().size(), ColorRgb{0,0,0});
		_globalLedBuffer = color;

		_ledStringColorOrder.clear();
		for (const Led& led : _ledString.leds())
		{
			_ledStringColorOrder.push_back(led.colorOrder);
		}

		// handle hwLedCount update
		_hwLedCount = qMax(getSetting(settings::type::DEVICE).object()["hardwareLedCount"].toInt(getLedCount()), getLedCount());

		// change in leds are also reflected in adjustment
		delete _raw2ledAdjustment;
		_raw2ledAdjustment = hyperhdr::createLedColorsAdjustment(static_cast<int>(_ledString.leds().size()), getSetting(settings::type::COLOR).object());

		// start cached effects
		_effectEngine->startCachedEffects();
	}
	else if(type == settings::type::DEVICE)
	{
		QJsonObject dev = config.object();

		// handle hwLedCount update
		_hwLedCount = qMax(dev["hardwareLedCount"].toInt(getLedCount()), getLedCount());

		// force ledString update, if device ByteOrder changed
		if(_ledDeviceWrapper->getColorOrder() != dev["colorOrder"].toString("rgb"))
		{
			_ledString = hyperhdr::createLedString(getSetting(settings::type::LEDS).array(), hyperhdr::createColorOrder(dev));
			_imageProcessor->setLedString(_ledString);

			_ledStringColorOrder.clear();
			for (const Led& led : _ledString.leds())
			{
				_ledStringColorOrder.push_back(led.colorOrder);
			}
		}

		// do always reinit until the led devices can handle dynamic changes
		dev["currentLedCount"] = _hwLedCount; // Inject led count info
		_ledDeviceWrapper->createLedDevice(dev);

		// TODO: Check, if framegrabber frequency is lower than latchtime..., if yes, stop
	}
	else if(type == settings::type::SMOOTHING)
	{
		_deviceSmooth->handleSettingsUpdate( type, config);
	}

	// update once to push single color sets / adjustments/ ledlayout resizes and update ledBuffer color
	update();
}

QJsonDocument HyperHdrInstance::getSetting(settings::type type) const
{
	return _settingsManager->getSetting(type);
}

bool HyperHdrInstance::saveSettings(const QJsonObject& config, bool correct)
{
	return _settingsManager->saveSettings(config, correct);
}

int HyperHdrInstance::getLatchTime() const
{
	return _ledDeviceWrapper->getLatchTime();
}

unsigned HyperHdrInstance::updateSmoothingConfig(unsigned id, int settlingTime_ms, double ledUpdateFrequency_hz, unsigned updateDelay, bool directMode)
{
	return _deviceSmooth->updateConfig(id, settlingTime_ms, ledUpdateFrequency_hz, updateDelay, directMode);
}

int HyperHdrInstance::getLedCount() const
{
	return static_cast<int>(_ledString.leds().size());
}

void HyperHdrInstance::setSourceAutoSelect(bool state)
{
	_muxer.setSourceAutoSelectEnabled(state);
}

bool HyperHdrInstance::setVisiblePriority(int priority)
{
	return _muxer.setPriority(priority);
}

bool HyperHdrInstance::sourceAutoSelectEnabled() const
{
	return _muxer.isSourceAutoSelectEnabled();
}

void HyperHdrInstance::setNewComponentState(hyperhdr::Components component, bool state)
{
	_componentRegister.setNewComponentState(component, state);

	if (!_bootEffect.isNull() && hyperhdr::Components::COMP_LEDDEVICE == component && state)
	{
		// initial startup effect
		if (QTime::currentTime() < _bootEffect)
			hyperhdr::handleInitialEffect(this, getSetting(settings::type::FGEFFECT).object());

		_bootEffect = QTime();
	}
}

std::map<hyperhdr::Components, bool> HyperHdrInstance::getAllComponents() const
{
	return _componentRegister.getRegister();
}

int HyperHdrInstance::isComponentEnabled(hyperhdr::Components comp) const
{
	return _componentRegister.isComponentEnabled(comp);
}

void HyperHdrInstance::registerInput(int priority, hyperhdr::Components component, const QString& origin, const QString& owner, unsigned smooth_cfg)
{
	_muxer.registerInput(priority, component, origin, owner, smooth_cfg);
}

bool HyperHdrInstance::setInput(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms, bool clearEffect)
{
	if(_muxer.setInput(priority, ledColors, timeout_ms))
	{
		// clear effect if this call does not come from an effect
		if(clearEffect)
		{
			_effectEngine->channelCleared(priority);
		}

		// if this priority is visible, update immediately
		if(priority == _muxer.getCurrentPriority())
		{
			update();
		}

		return true;
	}
	return false;
}

bool HyperHdrInstance::setInputImage(int priority, const Image<ColorRgb>& image, int64_t timeout_ms, bool clearEffect)
{
	if (!_muxer.hasPriority(priority))
	{
		emit GlobalSignals::getInstance()->globalRegRequired(priority);
		return false;
	}

	if(_muxer.setInputImage(priority, image, timeout_ms))
	{
		// clear effect if this call does not come from an effect
		if(clearEffect)
		{
			_effectEngine->channelCleared(priority);
		}

		// if this priority is visible, update immediately
		if(priority == _muxer.getCurrentPriority())
		{
			update();
		}

		return true;
	}
	return false;
}

bool HyperHdrInstance::setInputInactive(quint8 priority)
{
	return _muxer.setInputInactive(priority);
}

void HyperHdrInstance::setColor(int priority, const std::vector<ColorRgb> &ledColors, int timeout_ms, const QString &origin, bool clearEffects)
{
	// clear effect if this call does not come from an effect
	if (clearEffects)
	{
		_effectEngine->channelCleared(priority);
	}

	// create full led vector from single/multiple colors
	size_t size = _ledString.leds().size();
	std::vector<ColorRgb> newLedColors;
	while (true)
	{
		for (const auto &entry : ledColors)
		{
			newLedColors.emplace_back(entry);
			if (newLedColors.size() == size)
			{
				goto end;
			}
		}
	}
end:

	if (getPriorityInfo(priority).componentId != hyperhdr::COMP_COLOR)
	{
		clear(priority);
	}

	// register color
	registerInput(priority, hyperhdr::COMP_COLOR, origin);

	// write color to muxer
	setInput(priority, newLedColors, timeout_ms);
}

QStringList HyperHdrInstance::getAdjustmentIds() const
{
	return _raw2ledAdjustment->getAdjustmentIds();
}

ColorAdjustment * HyperHdrInstance::getAdjustment(const QString& id) const
{
	return _raw2ledAdjustment->getAdjustment(id);
}

void HyperHdrInstance::adjustmentsUpdated()
{
	emit adjustmentChanged();
	update();
}

bool HyperHdrInstance::clear(int priority, bool forceClearAll)
{
	bool isCleared = false;
	if (priority < 0)
	{
		_muxer.clearAll(forceClearAll);

		// send clearall signal to the effect engine
		_effectEngine->allChannelsCleared();
		isCleared = true;
	}
	else
	{
		// send clear signal to the effect engine
		// (outside the check so the effect gets cleared even when the effect is not sending colors)
		_effectEngine->channelCleared(priority);

		if (_muxer.clearInput(priority))
		{
			isCleared = true;
		}
	}
	return isCleared;
}

int HyperHdrInstance::getCurrentPriority() const
{
	return _muxer.getCurrentPriority();
}

bool HyperHdrInstance::isCurrentPriority(int priority) const
{
	return getCurrentPriority() == priority;
}

QList<int> HyperHdrInstance::getActivePriorities() const
{
	return _muxer.getPriorities();
}

HyperHdrInstance::InputInfo HyperHdrInstance::getPriorityInfo(int priority) const
{
	return _muxer.getInputInfo(priority);
}

std::list<EffectDefinition> HyperHdrInstance::getEffects() const
{
	return _effectEngine->getEffects();
}

std::list<ActiveEffectDefinition> HyperHdrInstance::getActiveEffects() const
{
	return _effectEngine->getActiveEffects();
}

QJsonObject HyperHdrInstance::getQJsonConfig() const
{
	return _settingsManager->getSettings();
}

int HyperHdrInstance::setEffect(const QString &effectName, int priority, int timeout, const QString & origin)
{
	return _effectEngine->runEffect(effectName, priority, timeout, origin);
}

int HyperHdrInstance::setEffect(const QString &effectName, const QJsonObject &args, int priority, int timeout, const QString &origin, const QString &imageData)
{
	return _effectEngine->runEffect(effectName, args, priority, timeout, origin, 0, imageData);
}

void HyperHdrInstance::setLedMappingType(int mappingType)
{
	if(mappingType != _imageProcessor->getUserLedMappingType())
	{
		_imageProcessor->setLedMappingType(mappingType);
		emit imageToLedsMappingChanged(mappingType);
	}
}

int HyperHdrInstance::getLedMappingType() const
{
	return _imageProcessor->getUserLedMappingType();
}

void HyperHdrInstance::setVideoModeHdr(int hdr)
{
	emit videoModeHdr(hdr);
}

int HyperHdrInstance::getCurrentVideoModeHdr() const
{
	return _currVideoModeHdr;
}

QString HyperHdrInstance::getActiveDeviceType() const
{
	return _ledDeviceWrapper->getActiveDeviceType();
}

void HyperHdrInstance::handleVisibleComponentChanged(hyperhdr::Components comp)
{
	_imageProcessor->setBlackbarDetectDisable((comp == hyperhdr::COMP_EFFECT));
	_imageProcessor->setHardLedMappingType((comp == hyperhdr::COMP_EFFECT) ? 0 : -1);
	_raw2ledAdjustment->setBacklightEnabled((comp != hyperhdr::COMP_COLOR && comp != hyperhdr::COMP_EFFECT));
}

void HyperHdrInstance::handlePriorityChangedLedDevice(const quint8& priority)
{
	int previousPriority = _muxer.getPreviousPriority();

	Debug(_log,"priority[%d], previousPriority[%d]", priority, previousPriority);
	if ( priority == PriorityMuxer::LOWEST_PRIORITY)
	{
		Error(_log,"No source left -> switch LED-Device off");
		emit _ledDeviceWrapper->switchOff();
	}
	else
	{
		if ( previousPriority == PriorityMuxer::LOWEST_PRIORITY )
		{
			Debug(_log,"new source available -> switch LED-Device on");
			emit _ledDeviceWrapper->switchOn();
		}
	}
}

void HyperHdrInstance::update()
{
	/// buffer for leds (with adjustment)
	std::vector<ColorRgb>	_ledBuffer = _globalLedBuffer;

	// Obtain the current priority channel
	int priority = _muxer.getCurrentPriority();
	const PriorityMuxer::InputInfo priorityInfo = _muxer.getInputInfo(priority);

	// copy image & process OR copy ledColors from muxer
	Image<ColorRgb> image = priorityInfo.image;

	if (image.width() > 1 || image.height() > 1)
	{
		emit currentImage(image);
		_ledBuffer = _imageProcessor->process(image);
	}
	else
	{
		_ledBuffer = priorityInfo.ledColors;
	}

	_globalLedBuffer = _ledBuffer;

	// emit rawLedColors before transform
	emit rawLedColors(_ledBuffer);

	_raw2ledAdjustment->applyAdjustment(_ledBuffer);

	int i = 0;
	for (ColorRgb& color : _ledBuffer)
	{
		// correct the color byte order
		switch (_ledStringColorOrder.at(i))
		{
		case ColorOrder::ORDER_RGB:
			// leave as it is
			break;
		case ColorOrder::ORDER_BGR:
			std::swap(color.red, color.blue);
			break;
		case ColorOrder::ORDER_RBG:
			std::swap(color.green, color.blue);
			break;
		case ColorOrder::ORDER_GRB:
			std::swap(color.red, color.green);
			break;
		case ColorOrder::ORDER_GBR:
			std::swap(color.red, color.green);
			std::swap(color.green, color.blue);
			break;

		case ColorOrder::ORDER_BRG:
			std::swap(color.red, color.blue);
			std::swap(color.green, color.blue);
			break;
		}
		i++;
	}

	// fill additional hardware LEDs with black
	if ( _hwLedCount > static_cast<int>(_ledBuffer.size()) )
	{
		_ledBuffer.resize(_hwLedCount, ColorRgb::BLACK);
	}

	// Write the data to the device
	if (_ledDeviceWrapper->enabled())
	{
		// Smoothing is disabled
		if  (! _deviceSmooth->enabled())
		{
			emit ledDeviceData(_ledBuffer);
		}
		else
		{			
		
			_deviceSmooth->selectConfig(priorityInfo.smooth_cfg);
		
			// feed smoothing in pause mode to maintain a smooth transition back to smooth mode
			if (_deviceSmooth->enabled() || _deviceSmooth->pause())
			{
				_deviceSmooth->updateLedValues(_ledBuffer);
			}
		}
	}
	#if 0
	else
	{
		//LEDDevice is disabled
		Debug(_log, "LEDDevice is disabled - no update required");
	}
	#endif
}


QString HyperHdrInstance::saveEffect(const QJsonObject& obj)
{
	return "";
}

QString HyperHdrInstance::deleteEffect(const QString& effectName)
{
	return "";
}
