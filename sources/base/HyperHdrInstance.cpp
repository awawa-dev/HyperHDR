// STL includes
#include <exception>
#include <sstream>

// QT includes
#include <QString>
#include <QStringList>
#include <QThread>

#include <HyperhdrConfig.h>

#include <base/HyperHdrInstance.h>
#include <base/MessageForwarder.h>
#include <base/ImageProcessor.h>
#include <base/ImageProcessingUnit.h>
#include <base/ColorAdjustment.h>

// utils
#include <utils/GlobalSignals.h>
#include <utils/Logger.h>

// LedDevice includes
#include <leddevice/LedDeviceWrapper.h>
#include <leddevice/LedDeviceFactory.h>

#include <base/MultiColorAdjustment.h>
#include <base/LinearSmoothing.h>

// effect engine includes
#include <effectengine/EffectEngine.h>

// settingsManagaer
#include <base/SettingsManager.h>

// BGEffectHandler
#include <base/BGEffectHandler.h>

// CaptureControl (Daemon capture)
#include <base/VideoControl.h>

// SystemControl (Daemon capture)
#include <base/SystemControl.h>

#include <base/GrabberWrapper.h>

// Boblight
#if defined(ENABLE_BOBLIGHT)	
#include <boblightserver/BoblightServer.h>
#else
class BoblightServer {};
#endif

#include <utils/RawUdpServer.h>



HyperHdrInstance::HyperHdrInstance(quint8 instance, bool readonlyMode, QString name)
	: QObject()
	, _instIndex(instance)
	, _bootEffect(QTime::currentTime().addSecs(5))
	, _imageProcessingUnit(nullptr)
	, _settingsManager(new SettingsManager(instance, this, readonlyMode))
	, _componentRegister(this)
	, _ledString(LedString::createLedString(getSetting(settings::type::LEDS).array(), LedString::createColorOrder(getSetting(settings::type::DEVICE).object())))
	, _imageProcessor(new ImageProcessor(_ledString, this))
	, _muxer(instance, static_cast<int>(_ledString.leds().size()), this)
	, _raw2ledAdjustment(MultiColorAdjustment::createLedColorsAdjustment(instance, static_cast<int>(_ledString.leds().size()), getSetting(settings::type::COLOR).object()))
	, _ledDeviceWrapper(nullptr)
	, _smoothing(nullptr)
	, _effectEngine(nullptr)
	, _messageForwarder(nullptr)
	, _log(Logger::getInstance(QString("HYPERHDR%1").arg(instance)))
	, _hwLedCount()
	, _ledGridSize(LedString::getLedLayoutGridSize(getSetting(settings::type::LEDS).array()))
	, _BGEffectHandler(nullptr)
	, _videoControl(nullptr)
	, _systemControl(nullptr)
	, _globalLedBuffer(_ledString.leds().size(), ColorRgb::BLACK)
	, _boblightServer(nullptr)
	, _rawUdpServer(nullptr)
	, _name((name.isEmpty()) ? QString("INSTANCE%1").arg(instance):name)
	, _readOnlyMode(readonlyMode)

{

}

bool HyperHdrInstance::isCEC()
{
	if ((_systemControl != nullptr && _systemControl->isCEC()) ||
		(_videoControl != nullptr && _videoControl->isCEC()) ||
		(GrabberWrapper::getInstance() != nullptr && GrabberWrapper::getInstance()->isCEC())
		)
		return true;

	return false;
}

void HyperHdrInstance::setSignalStateByCEC(bool enable)
{
	if (_systemControl != nullptr && _systemControl->isCEC())
	{
		emit _systemControl->setSysCaptureEnableSignal(enable);
	}
	if (_videoControl != nullptr && _videoControl->isCEC())
	{
		emit _videoControl->setUsbCaptureEnableSignal(enable);
	}
}


HyperHdrInstance::~HyperHdrInstance()
{
	freeObjects();
}

void HyperHdrInstance::start()
{
	Info(_log, "Led strip RGB order is: %s", QSTRING_CSTR(colorOrderToString(_ledString.colorOrder)));

	connect(_settingsManager, &SettingsManager::settingsChanged, this, &HyperHdrInstance::settingsChanged);

	if (!_raw2ledAdjustment->verifyAdjustments())
	{
		Warning(_log, "At least one led has no color calibration, please add all leds from your led layout to an 'LED index' field!");
	}

	// handle hwLedCount
	_hwLedCount = qMax(getSetting(settings::type::DEVICE).object()["hardwareLedCount"].toInt(1), getLedCount());

	connect(&_muxer, &PriorityMuxer::visiblePriorityChanged, this, &HyperHdrInstance::update);
	connect(&_muxer, &PriorityMuxer::visiblePriorityChanged, this, &HyperHdrInstance::handlePriorityChangedLedDevice);
	connect(&_muxer, &PriorityMuxer::visibleComponentChanged, this, &HyperHdrInstance::handleVisibleComponentChanged);

	// listens for ComponentRegister changes of COMP_ALL to perform core enable/disable actions


	// listen for settings updates of this instance (LEDS & COLOR)
	connect(_settingsManager, &SettingsManager::settingsChanged, this, &HyperHdrInstance::handleSettingsUpdate);

	// procesing unit
	_imageProcessingUnit = std::unique_ptr<ImageProcessingUnit>(new ImageProcessingUnit(this));
	connect(_imageProcessingUnit.get(), &ImageProcessingUnit::dataReadySignal, this, &HyperHdrInstance::updateResult);

	// initialize LED-devices
	QJsonObject ledDevice = getSetting(settings::type::DEVICE).object();
	ledDevice["currentLedCount"] = _hwLedCount; // Inject led count info

	// smoothing
	_smoothing = new LinearSmoothing(getSetting(settings::type::SMOOTHING), this);
	connect(this, &HyperHdrInstance::settingsChanged, _smoothing, &LinearSmoothing::handleSettingsUpdate);

	_ledDeviceWrapper = new LedDeviceWrapper(this);
	connect(this, &HyperHdrInstance::compStateChangeRequest, _ledDeviceWrapper, &LedDeviceWrapper::handleComponentState);
	connect(this, &HyperHdrInstance::ledDeviceData, _ledDeviceWrapper, &LedDeviceWrapper::updateLeds);
	_ledDeviceWrapper->createLedDevice(ledDevice);

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
	_videoControl = new VideoControl(this);

	_systemControl = new SystemControl(this);

	// forwards global signals to the corresponding slots
	connect(GlobalSignals::getInstance(), &GlobalSignals::registerGlobalInput, this, &HyperHdrInstance::registerInput);
	connect(GlobalSignals::getInstance(), &GlobalSignals::clearGlobalInput, this, &HyperHdrInstance::clear);
	connect(GlobalSignals::getInstance(), &GlobalSignals::setGlobalColor, this, &HyperHdrInstance::setColor);
	connect(GlobalSignals::getInstance(), &GlobalSignals::setGlobalImage, this, &HyperHdrInstance::setInputImage);

	// if there is no startup / background effect and no sending capture interface we probably want to push once BLACK (as PrioMuxer won't emit a priority change)
	update();

#ifdef ENABLE_BOBLIGHT
	// boblight, can't live in global scope as it depends on layout
	_boblightServer = new BoblightServer(this, getSetting(settings::type::BOBLSERVER));
	connect(this, &HyperHdrInstance::settingsChanged, _boblightServer, &BoblightServer::handleSettingsUpdate);
#endif

	_rawUdpServer = new RawUdpServer(this, getSetting(settings::type::RAWUDPSERVER));
	connect(this, &HyperHdrInstance::settingsChanged, _rawUdpServer, &RawUdpServer::handleSettingsUpdate);

	// instance initiated, enter thread event loop
	emit started();
}

void HyperHdrInstance::stop()
{
	Info(_log, "Stopping the instance...");

	emit finished();
	thread()->wait();
}

void HyperHdrInstance::freeObjects()
{
	Info(_log, "Freeing the objects...");

	// switch off all leds
	clear(-1, true);

	// delete components on exit
	delete _boblightServer;
	delete _rawUdpServer;
	delete _videoControl;
	delete _systemControl;
	delete _effectEngine;
	delete _raw2ledAdjustment;
	delete _messageForwarder;
	delete _settingsManager;
	delete _ledDeviceWrapper;
}

void HyperHdrInstance::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{

	//	std::cout << config.toJson().toStdString() << std::endl;

	if (type == settings::type::COLOR)
	{
		const QJsonObject obj = config.object();
		// change in color recreate ledAdjustments
		delete _raw2ledAdjustment;
		_raw2ledAdjustment = MultiColorAdjustment::createLedColorsAdjustment(_instIndex, static_cast<int>(_ledString.leds().size()), obj);

		if (!_raw2ledAdjustment->verifyAdjustments())
		{
			Warning(_log, "At least one led has no color calibration, please add all leds from your led layout to an 'LED index' field!");
		}

		emit imageToLedsMappingChanged(_imageProcessor->getLedMappingType());
	}
	else if (type == settings::type::LEDS)
	{
		const QJsonArray leds = config.array();

		// stop and cache all running effects, as effects depend heavily on LED-layout
		_effectEngine->cacheRunningEffects();

		// ledstring, img processor, muxer, ledGridSize (effect-engine image based effects), _ledBuffer and ByteOrder of ledstring
		_ledString = LedString::createLedString(leds, LedString::createColorOrder(getSetting(settings::type::DEVICE).object()));
		_imageProcessor->setLedString(_ledString);
		_muxer.updateLedColorsLength(static_cast<int>(_ledString.leds().size()));
		_ledGridSize = LedString::getLedLayoutGridSize(leds);

		std::vector<ColorRgb> color(_ledString.leds().size(), ColorRgb{ 0,0,0 });
		_globalLedBuffer = color;

		// handle hwLedCount update
		_hwLedCount = qMax(getSetting(settings::type::DEVICE).object()["hardwareLedCount"].toInt(1), getLedCount());

		// change in leds are also reflected in adjustment
		delete _raw2ledAdjustment;
		_raw2ledAdjustment = MultiColorAdjustment::createLedColorsAdjustment(_instIndex, static_cast<int>(_ledString.leds().size()), getSetting(settings::type::COLOR).object());

		// start cached effects
		_effectEngine->startCachedEffects();
	}
	else if (type == settings::type::DEVICE)
	{
		QJsonObject dev = config.object();

		// handle hwLedCount update
		_hwLedCount = qMax(dev["hardwareLedCount"].toInt(1), getLedCount());

		// force ledString update, if device ByteOrder changed
		if (_ledString.colorOrder != stringToColorOrder(dev["colorOrder"].toString("rgb")))
		{
			Info(_log, "New RGB order is: %s", QSTRING_CSTR(dev["colorOrder"].toString("rgb")));
			_ledString = LedString::createLedString(getSetting(settings::type::LEDS).array(), LedString::createColorOrder(dev));
			_imageProcessor->setLedString(_ledString);			
		}

		// do always reinit until the led devices can handle dynamic changes
		dev["currentLedCount"] = _hwLedCount; // Inject led count info
		_ledDeviceWrapper->createLedDevice(dev);

		// TODO: Check, if framegrabber frequency is lower than latchtime..., if yes, stop
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

void HyperHdrInstance::saveCalibration(QString saveData)
{
	_settingsManager->saveSetting(settings::type::VIDEODETECTION, saveData);
}

void HyperHdrInstance::setSmoothing(int time)
{
	_smoothing->updateCurrentConfig(time);
}

QJsonObject HyperHdrInstance::getAverageColor()
{
	QJsonObject ret;

	auto copy = _globalLedBuffer;
	long red = 0, green = 0, blue = 0, count = 0;
	
	for (const ColorRgb& c : copy)
	{
		red += c.red;
		green += c.green;
		blue += c.blue;

		count++;
	}

	if (!_ledDeviceWrapper->enabled())
	{
		red = green = blue = 0;
	}

	if (count > 0)
	{
		ret["red"] = static_cast<int>(red / count);
		ret["green"] = static_cast<int>(green / count);
		ret["blue"] = static_cast<int>(blue / count);
	}

	return ret;
}

unsigned HyperHdrInstance::updateSmoothingConfig(unsigned id, int settlingTime_ms, double ledUpdateFrequency_hz, bool directMode)
{
	unsigned retVal = id;

	if (QThread::currentThread() == _smoothing->thread())
		return _smoothing->updateConfig(id, settlingTime_ms, ledUpdateFrequency_hz, directMode);
	else
		QMetaObject::invokeMethod(_smoothing, "updateConfig",Qt::ConnectionType::BlockingQueuedConnection,
			Q_RETURN_ARG(unsigned, retVal), Q_ARG(unsigned, id), Q_ARG(int, settlingTime_ms), Q_ARG(double, ledUpdateFrequency_hz), Q_ARG(bool, directMode));
	
	return retVal;
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
			EffectEngine::handleInitialEffect(this, getSetting(settings::type::FGEFFECT).object());

		_bootEffect = QTime();
	}

	if (hyperhdr::Components::COMP_LEDDEVICE == component)
	{
		if (state)
			emit PerformanceCounters::getInstance()->newCounter(PerformanceReport(static_cast<int>(PerformanceReportType::LED), -1, "", -1, -1, -1, -1, getInstanceIndex()));
		else
			emit PerformanceCounters::getInstance()->removeCounter(static_cast<int>(PerformanceReportType::LED), getInstanceIndex());
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

void HyperHdrInstance::updateLedsValues(int priority, const std::vector<ColorRgb>& ledColors)
{
	_muxer.updateLedsValues(priority, ledColors);
}

bool HyperHdrInstance::setInput(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms, bool clearEffect)
{
	if (_muxer.setInput(priority, ledColors, timeout_ms))
	{
		// clear effect if this call does not come from an effect
		if (clearEffect)
		{
			_effectEngine->channelCleared(priority);
		}

		// if this priority is visible, update immediately
		if (priority == _muxer.getCurrentPriority())
		{
			update();
		}

		return true;
	}
	return false;
}

void HyperHdrInstance::saveGrabberParams(int hardware_brightness, int hardware_contrast, int hardware_saturation)
{
	QJsonDocument newSet = _settingsManager->getSetting(settings::type::VIDEOGRABBER);
	QJsonObject grabber = QJsonObject(newSet.object());
	grabber["hardware_brightness"] = hardware_brightness;
	grabber["hardware_contrast"] = hardware_contrast;
	grabber["hardware_saturation"] = hardware_saturation;
	_settingsManager->saveSetting(settings::type::VIDEOGRABBER, QJsonDocument(grabber).toJson(QJsonDocument::Compact));
}

bool HyperHdrInstance::setInputImage(int priority, const Image<ColorRgb>& image, int64_t timeout_ms, bool clearEffect)
{
	if (!_muxer.hasPriority(priority))
	{
		emit GlobalSignals::getInstance()->globalRegRequired(priority);
		return false;
	}

	bool isImage = image.width() > 1 || image.height() > 1;

	if (_muxer.setInputImage(priority, (receivers(SIGNAL(onCurrentImage())) > 0 || !isImage || timeout_ms == 0) ? image : Image<ColorRgb>(), timeout_ms))
	{
		// clear effect if this call does not come from an effect
		if (clearEffect)
		{
			_effectEngine->channelCleared(priority);
		}

		// if this priority is visible, update immediately
		if (priority == _muxer.getCurrentPriority())
		{
			if (isImage)			
				emit _imageProcessingUnit->queueImageSignal(priority, image);
			else
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

void HyperHdrInstance::setColor(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms, const QString& origin, bool clearEffects)
{
	if (ledColors.size() == 0)
		return;

	// clear effect if this call does not come from an effect
	if (clearEffects)
	{
		_effectEngine->channelCleared(priority);
	}

	// create full led vector from single/multiple colors
	std::vector<ColorRgb> newLedColors;
	auto currentCol = ledColors.begin();

	while (newLedColors.size() < _ledString.leds().size())
	{
		newLedColors.emplace_back(*currentCol);

		if (++currentCol == ledColors.end())
			currentCol = ledColors.begin();
	}

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

ColorAdjustment* HyperHdrInstance::getAdjustment(const QString& id) const
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

const HyperHdrInstance::InputInfo& HyperHdrInstance::getPriorityInfo(int priority) const
{
	return _muxer.getInputInfo(priority);
}

PriorityMuxer::InputInfo HyperHdrInstance::getCurrentPriorityInfo()
{
	PriorityMuxer::InputInfo val = _muxer.getInputInfo(getCurrentPriority());
	return val;
}

std::list<EffectDefinition> HyperHdrInstance::getEffects() const
{
	std::list<EffectDefinition> result;

	if (QThread::currentThread() == _effectEngine->thread())
		result = _effectEngine->getEffects();
	else
		QMetaObject::invokeMethod(_effectEngine, "getEffects", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(std::list<EffectDefinition>, result));

	return result;
}

std::list<ActiveEffectDefinition> HyperHdrInstance::getActiveEffects() const
{
	std::list<ActiveEffectDefinition> result;

	if (QThread::currentThread() == _effectEngine->thread())
		result = _effectEngine->getActiveEffects();
	else
		QMetaObject::invokeMethod(_effectEngine, "getActiveEffects", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(std::list<ActiveEffectDefinition>, result));

	return result;
}

QJsonObject HyperHdrInstance::getQJsonConfig() const
{
	QJsonObject obj;
	if (_settingsManager != NULL)
		return _settingsManager->getSettings();
	else
		return obj;
}

int HyperHdrInstance::setEffect(const QString& effectName, int priority, int timeout, const QString& origin)
{
	return _effectEngine->runEffect(effectName, priority, timeout, origin);
}

int HyperHdrInstance::setEffect(const QString& effectName, const QJsonObject& args, int priority, int timeout, const QString& origin, const QString& imageData)
{
	return _effectEngine->runEffect(effectName, args, priority, timeout, origin, 0, imageData);
}

void HyperHdrInstance::setLedMappingType(int mappingType)
{
	if (mappingType != _imageProcessor->getLedMappingType())
	{
		_imageProcessor->setLedMappingType(mappingType);
		emit imageToLedsMappingChanged(mappingType);
	}
}

int HyperHdrInstance::getLedMappingType() const
{
	return _imageProcessor->getLedMappingType();
}

QString HyperHdrInstance::getActiveDeviceType() const
{
	return _ledDeviceWrapper->getActiveDeviceType();
}

void HyperHdrInstance::handleVisibleComponentChanged(hyperhdr::Components comp)
{
	_imageProcessor->setBlackbarDetectDisable((comp == hyperhdr::COMP_EFFECT));
	_raw2ledAdjustment->setBacklightEnabled((comp != hyperhdr::COMP_COLOR && comp != hyperhdr::COMP_EFFECT));
}

void HyperHdrInstance::handlePriorityChangedLedDevice(const quint8& priority)
{
	int previousPriority = _muxer.getPreviousPriority();

	Info(_log, "New priority[%d], previous [%d]", priority, previousPriority);
	if (priority == PriorityMuxer::LOWEST_PRIORITY)
	{
		Warning(_log, "No source left -> switch LED-Device off");

		emit compStateChangeRequest(hyperhdr::COMP_LEDDEVICE, false);
		emit PerformanceCounters::getInstance()->removeCounter(static_cast<int>(PerformanceReportType::INSTANCE), getInstanceIndex());		
	}
	else
	{
		if (previousPriority == PriorityMuxer::LOWEST_PRIORITY)
		{
			Info(_log, "New source available -> switch LED-Device on");

			emit compStateChangeRequest(hyperhdr::COMP_LEDDEVICE, true);
			emit PerformanceCounters::getInstance()->newCounter(
				PerformanceReport(static_cast<int>(PerformanceReportType::INSTANCE), -1, _name, -1, -1, -1, -1, getInstanceIndex()));
		}
	}
}

ImageProcessor* HyperHdrInstance::getImageProcessor()
{
	return _imageProcessor;
}

void HyperHdrInstance::update()
{
	const PriorityMuxer::InputInfo& priorityInfo = _muxer.getInputInfo(_muxer.getCurrentPriority());
	emit _imageProcessingUnit->clearQueueImageSignal();
	emit _imageProcessingUnit->dataReadySignal(priorityInfo.ledColors);
}

void HyperHdrInstance::updateResult(std::vector<ColorRgb> _ledBuffer)
{
	// stats
	int64_t now = InternalClock::now();
	int64_t diff = now - _computeStats.statBegin;
	int64_t prevToken = _computeStats.token;

	if (_computeStats.token <= 0 || diff < 0)
	{
		_computeStats.token = PerformanceCounters::currentToken();
		_computeStats.statBegin = now;
		_computeStats.total = 1;
	}
	else if (prevToken != (_computeStats.token = PerformanceCounters::currentToken()))
	{

		if (diff >= 59000 && diff <= 65000)
			emit PerformanceCounters::getInstance()->newCounter(
				 PerformanceReport(static_cast<int>(PerformanceReportType::INSTANCE), _computeStats.token, _name, _computeStats.total / qMax(diff/1000.0, 1.0), _computeStats.total, 0, 0, getInstanceIndex()));
		
		_computeStats.statBegin = now;
		_computeStats.total = 1;
	}
	else
		_computeStats.total++;

	_globalLedBuffer = _ledBuffer;

	for (int disabledProcessing = 0; disabledProcessing < 2; disabledProcessing++)
	{
		if (disabledProcessing == 1)
			_raw2ledAdjustment->applyAdjustment(_ledBuffer);

		if (_ledString.hasDisabled)
		{
			auto ledIter = _ledString.leds().begin();
			for (ColorRgb& color : _ledBuffer)
				if (ledIter != _ledString.leds().end())
				{
					if ((*ledIter).disabled)
						color = ColorRgb::BLACK;
					++ledIter;
				}
		}

		if (disabledProcessing == 0)
			emit rawLedColors(_ledBuffer);	
	}

	if (_ledString.colorOrder != ColorOrder::ORDER_RGB)
	{
		for (ColorRgb& color : _ledBuffer)
		{
			// correct the color byte order
			switch (_ledString.colorOrder)
			{
				case ColorOrder::ORDER_RGB:
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
		}
	}

	// fill additional hardware LEDs with black
	if (_hwLedCount > static_cast<int>(_ledBuffer.size()))
	{
		_ledBuffer.resize(_hwLedCount, ColorRgb::BLACK);
	}

	// Write the data to the device
	if (_ledDeviceWrapper->enabled())
	{
		// Smoothing is disabled
		if (!_smoothing->enabled())
		{
			emit ledDeviceData(_ledBuffer);
		}
		else
		{
			int priority = _muxer.getCurrentPriority();
			const PriorityMuxer::InputInfo& priorityInfo = _muxer.getInputInfo(priority);

			_smoothing->selectConfig(priorityInfo.smooth_cfg, false);

			// feed smoothing in pause mode to maintain a smooth transition back to smooth mode
			if (_smoothing->enabled() || _smoothing->pause())
			{
				_smoothing->updateLedValues(_ledBuffer);
			}
		}
	}
}


QString HyperHdrInstance::saveEffect(const QJsonObject& obj)
{
	return "";
}

QString HyperHdrInstance::deleteEffect(const QString& effectName)
{
	return "";
}


void HyperHdrInstance::identifyLed(const QJsonObject& params)
{
	_ledDeviceWrapper->handleComponentState(hyperhdr::Components::COMP_LEDDEVICE, true);
	_ledDeviceWrapper->identifyLed(params);
}
