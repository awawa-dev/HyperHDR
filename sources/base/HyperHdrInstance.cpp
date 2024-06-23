/* HyperHdrInstance.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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
	#include <QString>
	#include <QStringList>
	#include <QThread>
	#include <QPair>

	#include <exception>
	#include <sstream>	
#endif

#include <HyperhdrConfig.h>
#include <base/HyperHdrInstance.h>
#include <base/ImageToLedManager.h>
#include <utils/GlobalSignals.h>
#include <led-drivers/LedDeviceWrapper.h>
#include <led-strip/LedCalibration.h>
#include <base/Smoothing.h>
#include <effects/EffectEngine.h>
#include <base/InstanceConfig.h>
#include <base/VideoControl.h>
#include <base/SystemControl.h>
#include <base/GrabberWrapper.h>
#include <base/RawUdpServer.h>
#include <base/ComponentController.h>
#include <base/Muxer.h>
#include <base/SoundCapture.h>
#include <base/SystemWrapper.h>
#include <base/GrabberHelper.h>
#include <utils/Logger.h>

std::atomic<bool> HyperHdrInstance::_signalTerminate(false);
std::atomic<int>  HyperHdrInstance::_totalRunningCount(0);

HyperHdrInstance::HyperHdrInstance(quint8 instance, bool disableOnStartup, QString name)
	: QObject()
	, _instIndex(instance)
	, _bootEffect(QTime::currentTime().addSecs(5))
	, _ledString()
	, _instanceConfig(nullptr)
	, _componentController(nullptr)
	, _imageProcessor(nullptr)
	, _muxer(nullptr)
	, _ledColorCalibration(nullptr)
	, _ledDeviceWrapper(nullptr)
	, _smoothing(nullptr)
	, _effectEngine(nullptr)
	, _videoControl(nullptr)
	, _systemControl(nullptr)
	, _rawUdpServer(nullptr)
	, _log(nullptr)
	, _hwLedCount()
	, _ledGridSize()
	, _currentLedColors()
	, _name((name.isEmpty()) ? QString("INSTANCE%1").arg(instance) : name)
	, _disableOnStartup(disableOnStartup)
{
	_totalRunningCount++;
}

HyperHdrInstance::~HyperHdrInstance()
{
	Info(_log, "Stopping and releasing components of a HyperHDR instance...");

	disconnect(GlobalSignals::getInstance(), nullptr, this, nullptr);
	disconnect(this, nullptr, nullptr, nullptr);

	if (_muxer != nullptr)
		clear(-1, true);

	Info(_log, "[ 1/9] Releasing HyperHDR%i->UdpServer...", _instIndex);
	_rawUdpServer = nullptr;
	Info(_log, "[ 2/9] Releasing HyperHDR%i->VideoControl...", _instIndex);
	_videoControl = nullptr;
	Info(_log, "[ 3/9] Releasing HyperHDR%i->SystemControl...", _instIndex);
	_systemControl = nullptr;
	Info(_log, "[ 4/9] Releasing HyperHDR%i->EffectEngine...", _instIndex);
	_effectEngine = nullptr;
	Info(_log, "[ 5/9] Releasing HyperHDR%i->ColorCalibration...", _instIndex);
	_ledColorCalibration = nullptr;
	Info(_log, "[ 6/9] Releasing HyperHDR%i->InstanceConfiguration...", _instIndex);
	_instanceConfig = nullptr;
	Info(_log, "[ 7/9] Releasing HyperHDR%i->LED driver wrapper...", _instIndex);
	_ledDeviceWrapper = nullptr;
	Info(_log, "[ 8/9] Releasing HyperHDR%i->ComponentController...", _instIndex);
	_componentController = nullptr;
	Info(_log, "[ 9/9] Releasing HyperHDR%i->Muxer...", _instIndex);
	_muxer = nullptr;

	_totalRunningCount--;

	Warning(_log, "The instance has been released succesfully [left: %i]", (int)_totalRunningCount);
}

void HyperHdrInstance::start()
{
	_log = Logger::getInstance(QString("HYPERHDR%1").arg(_instIndex));

	if (_signalTerminate)
	{
		Warning(_log, "Signal terminate detected. Skipping start of the instance");
		return;
	}

	Info(_log, "Starting the instance");

	_instanceConfig = std::unique_ptr<InstanceConfig>(new InstanceConfig(false, _instIndex, this));
	_componentController = std::unique_ptr<ComponentController>(new ComponentController(this, _disableOnStartup));
	connect(_componentController.get(), &ComponentController::SignalComponentStateChanged, this, &HyperHdrInstance::SignalComponentStateChanged);
	_ledString = LedString::createLedString(getSetting(settings::type::LEDS).array(), LedString::createColorOrder(getSetting(settings::type::DEVICE).object()));
	_muxer = std::unique_ptr<Muxer>(new Muxer(_instIndex, static_cast<int>(_ledString.leds().size()), this));
	_imageProcessor = std::unique_ptr<ImageToLedManager>(new ImageToLedManager(_ledString, this));
	connect(_muxer.get(), &Muxer::SignalPrioritiesChanged, this, &HyperHdrInstance::SignalPrioritiesChanged);
	connect(_muxer.get(), &Muxer::SignalVisiblePriorityChanged, this, &HyperHdrInstance::SignalVisiblePriorityChanged);
	connect(_muxer.get(), &Muxer::SignalVisibleComponentChanged, this, &HyperHdrInstance::SignalVisibleComponentChanged);
	_ledColorCalibration = std::unique_ptr<LedCalibration>(new LedCalibration(_instIndex, static_cast<int>(_ledString.leds().size()), getSetting(settings::type::COLOR).object()));
	_ledGridSize = LedString::getLedLayoutGridSize(getSetting(settings::type::LEDS).array());
	_currentLedColors = std::vector<ColorRgb>(_ledString.leds().size(), ColorRgb::BLACK);

	Info(_log, "Led strip RGB order is: %s", QSTRING_CSTR(LedString::colorOrderToString(_ledString.colorOrder)));

	connect(_instanceConfig.get(), &InstanceConfig::SignalInstanceSettingsChanged, this, &HyperHdrInstance::SignalInstanceSettingsChanged);

	if (!_ledColorCalibration->verifyAdjustments())
	{
		Warning(_log, "At least one led has no color calibration, please add all leds from your led layout to an 'LED index' field!");
	}

	// handle hwLedCount
	_hwLedCount = qMax(getSetting(settings::type::DEVICE).object()["hardwareLedCount"].toInt(1), getLedCount());

	connect(this, &HyperHdrInstance::SignalVisiblePriorityChanged, this, &HyperHdrInstance::update);
	connect(this, &HyperHdrInstance::SignalVisiblePriorityChanged, this, &HyperHdrInstance::handlePriorityChangedLedDevice);
	connect(this, &HyperHdrInstance::SignalVisibleComponentChanged, this, &HyperHdrInstance::handleVisibleComponentChanged);

	// listen for settings updates of this instance (LEDS & COLOR)
	connect(_instanceConfig.get(), &InstanceConfig::SignalInstanceSettingsChanged, this, &HyperHdrInstance::handleSettingsUpdate);

	// initialize LED-devices
	QJsonObject ledDevice = getSetting(settings::type::DEVICE).object();
	ledDevice["currentLedCount"] = _hwLedCount; // Inject led count info

	// smoothing
	_smoothing = std::unique_ptr<Smoothing>(new Smoothing(getSetting(settings::type::SMOOTHING), this));
	connect(this, &HyperHdrInstance::SignalInstanceSettingsChanged, _smoothing.get(), &Smoothing::handleSettingsUpdate);
	connect(this, &HyperHdrInstance::SignalSmoothingClockTick, _smoothing.get(), &Smoothing::SignalMasterClockTick, Qt::DirectConnection);

	_ledDeviceWrapper = std::unique_ptr<LedDeviceWrapper>(new LedDeviceWrapper(this));
	connect(this, &HyperHdrInstance::SignalRequestComponent, _ledDeviceWrapper.get(), &LedDeviceWrapper::handleComponentState);
	_ledDeviceWrapper->createLedDevice(ledDevice, _smoothing->GetSuggestedInterval(), _disableOnStartup);

	// create the effect engine; needs to be initialized after smoothing!
	_effectEngine = std::unique_ptr<EffectEngine>(new EffectEngine(this));
	connect(this, &HyperHdrInstance::SignalVisiblePriorityChanged, _effectEngine.get(), &EffectEngine::visiblePriorityChanged);

	// create the Daemon capture interface
	_videoControl = std::unique_ptr<VideoControl>(new VideoControl(this));

	_systemControl = std::unique_ptr<SystemControl>(new SystemControl(this));

	// forwards global signals to the corresponding slots
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalClearGlobalInput, this, &HyperHdrInstance::clear);
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalColor, this, &HyperHdrInstance::signalSetGlobalColorHandler);
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &HyperHdrInstance::signalSetGlobalImageHandler);

	// if there is no startup / background effect and no sending capture interface we probably want to push once BLACK (as PrioMuxer won't emit a priority change)
	update();

	_rawUdpServer = std::unique_ptr<RawUdpServer>(new RawUdpServer(this, getSetting(settings::type::RAWUDPSERVER)));
	connect(this, &HyperHdrInstance::SignalInstanceSettingsChanged, _rawUdpServer.get(), &RawUdpServer::handleSettingsUpdate);

	// handle background effect
	QUEUE_CALL_2(this, handleSettingsUpdate,
		settings::type, settings::type::BGEFFECT,
		QJsonDocument, getSetting(settings::type::BGEFFECT));

	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, _instIndex, true);

	// instance initiated, enter thread event loop
	emit SignalInstanceJustStarted();

	if (_disableOnStartup)
	{
		_componentController->setNewComponentState(hyperhdr::COMP_ALL, false);
		Warning(_log, "The user has disabled LEDs auto-start in the configuration (interface: 'General' tab)");
	}

	// exit
	Info(_log, "The instance is running");
}


void HyperHdrInstance::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{

	//	std::cout << config.toJson().toStdString() << std::endl;

	if (type == settings::type::COLOR)
	{
		const QJsonObject obj = config.object();
		// change in color recreate ledAdjustments
		_ledColorCalibration = std::unique_ptr<LedCalibration>(new LedCalibration(_instIndex, static_cast<int>(_ledString.leds().size()), obj));

		if (!_ledColorCalibration->verifyAdjustments())
		{
			Warning(_log, "At least one led has no color calibration, please add all leds from your led layout to an 'LED index' field!");
		}

		emit SignalImageToLedsMappingChanged(_imageProcessor->getLedMappingType());
	}
	else if (type == settings::type::LEDS)
	{
		const QJsonArray leds = config.array();

		// ledstring, img processor, muxer, ledGridSize (effect-engine image based effects), _ledBuffer and ByteOrder of ledstring
		_ledString = LedString::createLedString(leds, LedString::createColorOrder(getSetting(settings::type::DEVICE).object()));
		_imageProcessor->setLedString(_ledString);
		_ledGridSize = LedString::getLedLayoutGridSize(leds);

		std::vector<ColorRgb> color(_ledString.leds().size(), ColorRgb{ 0,0,0 });
		_currentLedColors = color;

		// handle hwLedCount update
		_hwLedCount = qMax(getSetting(settings::type::DEVICE).object()["hardwareLedCount"].toInt(1), getLedCount());

		// change in leds are also reflected in adjustment
		_ledColorCalibration = std::unique_ptr<LedCalibration>(new LedCalibration(_instIndex, static_cast<int>(_ledString.leds().size()), getSetting(settings::type::COLOR).object()));
	}
	else if (type == settings::type::DEVICE)
	{
		QJsonObject dev = config.object();

		// handle hwLedCount update
		_hwLedCount = qMax(dev["hardwareLedCount"].toInt(1), getLedCount());

		// force ledString update, if device ByteOrder changed
		if (_ledString.colorOrder != LedString::stringToColorOrder(dev["colorOrder"].toString("rgb")))
		{
			Info(_log, "New RGB order is: %s", QSTRING_CSTR(dev["colorOrder"].toString("rgb")));
			_ledString = LedString::createLedString(getSetting(settings::type::LEDS).array(), LedString::createColorOrder(dev));
			_imageProcessor->setLedString(_ledString);
		}

		// do always reinit until the led devices can handle dynamic changes
		dev["currentLedCount"] = _hwLedCount; // Inject led count info
		_ledDeviceWrapper->createLedDevice(dev, _smoothing->GetSuggestedInterval(), false);
	}
	else if (type == settings::type::BGEFFECT || type == settings::type::FGEFFECT)
	{
		bool isBootEffect = (type == settings::type::FGEFFECT);
		int effectPriority = (isBootEffect) ? 0 : 254;

		if (isBootEffect)
		{
			if (QTime::currentTime() < _bootEffect)
				_bootEffect = QTime::currentTime();
			else
				return;
		}
		else
			clear(effectPriority);

		const QJsonObject& effectObject = config.object();

		if (effectObject["enable"].toBool(true))
		{
			const char* effectDesc = (isBootEffect) ? "Boot effect" : "Background effect";
			const QString effectType = effectObject["type"].toString("effect");
			const QString effectAnimation = effectObject["effect"].toString("Rainbow swirl fast");
			const QJsonValue effectColors = effectObject["color"];
			int effectDuration = (isBootEffect) ? effectObject["duration_ms"].toInt(3000) : -1;

			if (isBootEffect && effectDuration <= 0)
			{
				effectDuration = 3000;
				Warning(_log, "Boot effect duration 'infinity' is forbidden, set to default value %d ms", effectDuration);
			}

			if (effectType.contains("color"))
			{
				const QJsonArray& COLORS = effectColors.toArray();

				if (COLORS.size() < 3)
					return;

				std::vector<ColorRgb> colors = {
					ColorRgb { (uint8_t)(COLORS[0].toInt(0)), (uint8_t)(COLORS[1].toInt(0)), (uint8_t)(COLORS[2].toInt(0))}
				};

				setColor(effectPriority, colors, effectDuration);
				Info(_log, "%s: color set to [%d, %d, %d]", effectDesc, colors[0].red, colors[0].green, colors[0].blue);
			}
			else
			{
				int result = setEffect(effectAnimation, effectPriority, effectDuration);
				Info(_log, "%s: animation set to '%s' [%s]", effectDesc, QSTRING_CSTR(effectAnimation), ((result == 0) ? "started" : "failed"));
			}
		}
	}

	if (type == settings::type::DEVICE || type == settings::type::LEDS)
		update();
}

void HyperHdrInstance::signalTerminateTriggered()
{
	_signalTerminate = true;
	LedDevice::signalTerminateTriggered();
}

bool HyperHdrInstance::isTerminated()
{
	return _signalTerminate;
}

int HyperHdrInstance::getTotalRunningCount()
{
	return _totalRunningCount;
}

QJsonDocument HyperHdrInstance::getSetting(settings::type type) const
{
	return _instanceConfig->getSetting(type);
}

void HyperHdrInstance::setSetting(settings::type type, QString config)
{
	return _instanceConfig->saveSetting(type, config);
}

bool HyperHdrInstance::saveSettings(const QJsonObject& config, bool correct)
{
	return _instanceConfig->saveSettings(config, correct);
}

void HyperHdrInstance::saveCalibration(QString saveData)
{
	_instanceConfig->saveSetting(settings::type::VIDEODETECTION, saveData);
}

void HyperHdrInstance::setSmoothing(int time)
{
	_smoothing->UpdateCurrentConfig(time);
}

QJsonObject HyperHdrInstance::getAverageColor()
{
	QJsonObject ret;

	long red = 0, green = 0, blue = 0, count = 0;

	for (const ColorRgb& c : _currentLedColors)
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

unsigned HyperHdrInstance::addEffectConfig(unsigned id, int settlingTime_ms, double ledUpdateFrequency_hz, bool pause)
{
	return _smoothing->AddEffectConfig(id, settlingTime_ms, ledUpdateFrequency_hz, pause);
}

int HyperHdrInstance::getLedCount() const
{
	return static_cast<int>(_ledString.leds().size());
}

bool HyperHdrInstance::getReadOnlyMode() const
{
	return _instanceConfig->isReadOnlyMode();
}

void HyperHdrInstance::setSourceAutoSelect(bool state)
{
	_muxer->setSourceAutoSelectEnabled(state);
}

bool HyperHdrInstance::setVisiblePriority(int priority)
{
	return _muxer->setPriority(priority);
}

bool HyperHdrInstance::sourceAutoSelectEnabled() const
{
	return _muxer->isSourceAutoSelectEnabled();
}

void HyperHdrInstance::setNewComponentState(hyperhdr::Components component, bool state)
{
	if (_componentController == nullptr || _muxer == nullptr)
		return;

	_componentController->setNewComponentState(component, state);

	if (hyperhdr::Components::COMP_LEDDEVICE == component && state)
	{
		QUEUE_CALL_2(this, handleSettingsUpdate,
			settings::type, settings::type::FGEFFECT,
			QJsonDocument, getSetting(settings::type::FGEFFECT));
	}

	if (hyperhdr::Components::COMP_LEDDEVICE == component)
	{
		emit GlobalSignals::getInstance()->SignalPerformanceStateChanged(state, hyperhdr::PerformanceReportType::LED, getInstanceIndex());

		emit SignalInstancePauseChanged(_instIndex, state);
	}
}

int HyperHdrInstance::isComponentEnabled(hyperhdr::Components comp) const
{
	return _componentController->isComponentEnabled(comp);
}

void HyperHdrInstance::registerInput(int priority, hyperhdr::Components component, const QString& origin, const QString& owner, unsigned smooth_cfg)
{
	_muxer->registerInput(priority, component, origin, ColorRgb::BLACK, smooth_cfg, owner);
}

bool HyperHdrInstance::setInputLeds(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms, bool clearEffect)
{
	if (_muxer->setInput(priority, timeout_ms))
	{
		// clear effect if this call does not come from an effect
		if (clearEffect)
		{
			_effectEngine->channelCleared(priority);
		}

		// if this priority is visible, update immediately
		if (priority == _muxer->getCurrentPriority())
		{
			if (_currentLedColors.size() == ledColors.size())
				_currentLedColors = ledColors;
			else
				Warning(_log, "Discrepancy between the number of colors in the input (%i) and the current ones (%i)",
					ledColors.size(), _currentLedColors.size());

			update();
		}

		return true;
	}
	return false;
}

void HyperHdrInstance::signalSetGlobalImageHandler(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin, QString clientDescription)
{
	if (!_muxer->hasPriority(priority))
	{
		_muxer->registerInput(priority, origin, clientDescription);
	}

	setInputImage(priority, image, timeout_ms);
}

void HyperHdrInstance::setInputImage(int priority, const Image<ColorRgb>& image, int64_t timeout_ms, bool clearEffect)
{
	if (!_muxer->hasPriority(priority))
	{
		return;
	}

	bool isImage = image.width() > 1 && image.height() > 1;

	if (_muxer->setInput(priority, timeout_ms))
	{
		// clear effect if this call does not come from an effect
		if (clearEffect)
		{
			_effectEngine->channelCleared(priority);
		}

		// if this priority is visible, update immediately
		if (priority == _muxer->getCurrentPriority())
		{
			if (isImage)
			{
				std::vector<ColorRgb> colors;

				_imageProcessor->processFrame(colors, image);

				if (colors.size() > 0)
				{
					updateResult(colors);
					emit SignalInstanceImageUpdated(image);
				}
			}
			else
				update();
		}
	}
}

bool HyperHdrInstance::setInputInactive(quint8 priority)
{
	return _muxer->setInputInactive(priority);
}

void HyperHdrInstance::signalSetGlobalColorHandler(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, hyperhdr::Components origin, QString clientDescription)
{
	setColor(priority, ledColor, timeout_ms, clientDescription, true);
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

	if (getComponentForPriority(priority) != hyperhdr::COMP_COLOR)
	{
		clear(priority);
	}
	if (getCurrentPriority() == priority)
	{
		emit SignalColorIsSet(ledColors[0], timeout_ms);
	}
	// register color
	_muxer->registerInput(priority, hyperhdr::COMP_COLOR, origin, ledColors[0]);
	_muxer->setInput(priority, timeout_ms);
	update();
}

void HyperHdrInstance::updateAdjustments(const QJsonObject& config)
{
	_ledColorCalibration->updateConfig(config);
	emit SignalAdjustmentUpdated(_ledColorCalibration->getAdjustmentState());
	update();
}

bool HyperHdrInstance::clear(int priority, bool forceClearAll)
{
	bool isCleared = false;
	if (priority < 0)
	{
		_muxer->clearAll(forceClearAll);

		// send clearall signal to the effect engine
		_effectEngine->allChannelsCleared();
		isCleared = true;
	}
	else
	{
		// send clear signal to the effect engine
		// (outside the check so the effect gets cleared even when the effect is not sending colors)
		_effectEngine->channelCleared(priority);

		if (_muxer->clearInput(priority))
		{
			isCleared = true;
		}
	}
	return isCleared;
}

int HyperHdrInstance::getCurrentPriority() const
{
	return _muxer->getCurrentPriority();
}

bool HyperHdrInstance::hasPriority(int priority)
{
	return _muxer->hasPriority(priority);
}

hyperhdr::Components HyperHdrInstance::getComponentForPriority(int priority)
{
	return _muxer->getInputInfo(priority).componentId;
}

hyperhdr::Components HyperHdrInstance::getCurrentPriorityActiveComponent()
{
	return _muxer->getInputInfo(getCurrentPriority()).componentId;
}

std::list<EffectDefinition> HyperHdrInstance::getEffects() const
{
	return _effectEngine->getEffects();
}

void HyperHdrInstance::putJsonConfig(QJsonObject& info) const
{
	info = _instanceConfig->getSettings();
}

int HyperHdrInstance::setEffect(const QString& effectName, int priority, int timeout, const QString& origin)
{
	if (_effectEngine != nullptr)
		return _effectEngine->runEffect(effectName, priority, timeout, origin);
	else
		return 0;
}

void HyperHdrInstance::setLedMappingType(int mappingType)
{
	if (mappingType != _imageProcessor->getLedMappingType())
	{
		_imageProcessor->setLedMappingType(mappingType);
		emit SignalImageToLedsMappingChanged(mappingType);
	}
}

void HyperHdrInstance::handleVisibleComponentChanged(hyperhdr::Components comp)
{
	_imageProcessor->setBlackbarDetectDisable((comp == hyperhdr::COMP_EFFECT));
	_ledColorCalibration->setBacklightEnabled((comp != hyperhdr::COMP_COLOR && comp != hyperhdr::COMP_EFFECT));
}

void HyperHdrInstance::handlePriorityChangedLedDevice(const quint8& priority)
{
	if (_signalTerminate)
		return;

	int previousPriority = _muxer->getPreviousPriority();

	Info(_log, "New priority[%d], previous [%d]", priority, previousPriority);
	if (priority == Muxer::LOWEST_PRIORITY)
	{
		Warning(_log, "No source left -> switch LED-Device off");

		emit SignalRequestComponent(hyperhdr::COMP_LEDDEVICE, false);
		emit GlobalSignals::getInstance()->SignalPerformanceStateChanged(false, hyperhdr::PerformanceReportType::INSTANCE, getInstanceIndex());
	}
	else
	{
		if (previousPriority == Muxer::LOWEST_PRIORITY)
		{
			Info(_log, "New source available -> switch LED-Device on");
			if (!isComponentEnabled(hyperhdr::Components::COMP_ALL))
			{
				Warning(_log, "Components are disabled: ignoring switching LED-Device on");
			}
			else
			{
				emit SignalRequestComponent(hyperhdr::COMP_LEDDEVICE, true);
				emit GlobalSignals::getInstance()->SignalPerformanceStateChanged(true, hyperhdr::PerformanceReportType::INSTANCE, getInstanceIndex(), _name);
			}
		}
	}
}

void HyperHdrInstance::update()
{
	if (_signalTerminate)
		return;

	const Muxer::InputInfo& priorityInfo = _muxer->getInputInfo(_muxer->getCurrentPriority());
	if (priorityInfo.componentId == hyperhdr::Components::COMP_COLOR)
	{
		std::fill(_currentLedColors.begin(), _currentLedColors.end(), priorityInfo.staticColor);
	}
	updateResult(_currentLedColors);
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
			emit GlobalSignals::getInstance()->SignalPerformanceNewReport(
				PerformanceReport(hyperhdr::PerformanceReportType::INSTANCE, _computeStats.token, _name, _computeStats.total / qMax(diff / 1000.0, 1.0), _computeStats.total, 0, 0, getInstanceIndex()));

		_computeStats.statBegin = now;
		_computeStats.total = 1;
	}
	else
		_computeStats.total++;

	_currentLedColors = _ledBuffer;

	for (int disabledProcessing = 0; disabledProcessing < 2; disabledProcessing++)
	{
		if (disabledProcessing == 1)
		{
			_ledColorCalibration->applyAdjustment(_ledBuffer);
		}

		if (_ledString.hasDisabled)
		{
			auto ledsCurrent = _ledString.leds().begin();
			auto ledsEnd = _ledString.leds().end();
			auto colorsCurrent = _ledBuffer.begin();
			auto colorsEnd = _ledBuffer.end();
			for (; colorsCurrent != colorsEnd && ledsCurrent != ledsEnd; ++colorsCurrent, ++ledsCurrent)
				if ((*ledsCurrent).disabled)
					(*colorsCurrent) = ColorRgb::BLACK;
		}

		if (disabledProcessing == 0)
			emit SignalRawColorsChanged(_ledBuffer);
	}

	if (_ledString.colorOrder != LedString::ColorOrder::ORDER_RGB)
	{
		for (ColorRgb& color : _ledBuffer)
		{
			// correct the color byte order
			switch (_ledString.colorOrder)
			{
				case LedString::ColorOrder::ORDER_RGB:
					break;
				case LedString::ColorOrder::ORDER_BGR:
					std::swap(color.red, color.blue);
					break;
				case LedString::ColorOrder::ORDER_RBG:
					std::swap(color.green, color.blue);
					break;
				case LedString::ColorOrder::ORDER_GRB:
					std::swap(color.red, color.green);
					break;
				case LedString::ColorOrder::ORDER_GBR:
					std::swap(color.red, color.green);
					std::swap(color.green, color.blue);
					break;

				case LedString::ColorOrder::ORDER_BRG:
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
		if (!_smoothing->isEnabled())
		{
			emit SignalUpdateLeds(_ledBuffer);
		}
		else
		{
			int priority = _muxer->getCurrentPriority();
			const Muxer::InputInfo& priorityInfo = _muxer->getInputInfo(priority);

			_smoothing->SelectConfig(priorityInfo.smooth_cfg, false);

			// feed smoothing in pause mode to maintain a smooth transition back to smooth mode
			if (_smoothing->isEnabled() || _smoothing->isPaused())
			{
				_smoothing->UpdateLedValues(_ledBuffer);
			}
		}
	}
}

void HyperHdrInstance::identifyLed(const QJsonObject& params)
{
	_ledDeviceWrapper->handleComponentState(hyperhdr::Components::COMP_LEDDEVICE, true);
	_ledDeviceWrapper->identifyLed(params);
}

void HyperHdrInstance::putJsonInfo(QJsonObject& info, bool full)
{
	uint64_t now = InternalClock::now();
	int currentPriority = getCurrentPriority();

	////////////////////////////////////
	// collect priorities information //
	////////////////////////////////////

	QJsonArray priorities;
	for (const Muxer::InputInfo& priorityInfo : _muxer->getInputInfoTable())
		if (priorityInfo.priority != Muxer::LOWEST_PRIORITY)
		{
			QJsonObject item;
			item["priority"] = priorityInfo.priority;
			if (priorityInfo.timeout > 0)
				item["duration_ms"] = int(priorityInfo.timeout - now);

			// owner has optional informations to the component
			if (!priorityInfo.owner.isEmpty())
				item["owner"] = priorityInfo.owner;

			item["componentId"] = QString(hyperhdr::componentToIdString(priorityInfo.componentId));
			item["origin"] = priorityInfo.origin;
			item["active"] = (priorityInfo.timeout >= -1);
			item["visible"] = (priorityInfo.priority == currentPriority);

			if (priorityInfo.componentId == hyperhdr::COMP_COLOR)
			{
				QJsonObject LEDcolor;

				// add RGB Value to Array
				QJsonArray RGBValue;
				RGBValue.append(priorityInfo.staticColor.red);
				RGBValue.append(priorityInfo.staticColor.green);
				RGBValue.append(priorityInfo.staticColor.blue);
				LEDcolor.insert("RGB", RGBValue);

				uint16_t Hue;
				float Saturation, Luminace;

				// add HSL Value to Array
				QJsonArray HSLValue;
				ColorRgb::rgb2hsl(priorityInfo.staticColor.red,
					priorityInfo.staticColor.green,
					priorityInfo.staticColor.blue,
					Hue, Saturation, Luminace);

				HSLValue.append(Hue);
				HSLValue.append(Saturation);
				HSLValue.append(Luminace);
				LEDcolor.insert("HSL", HSLValue);

				item["value"] = LEDcolor;
			}


			(priorityInfo.priority == currentPriority)
				? priorities.prepend(item)
				: priorities.append(item);
		}

	info["priorities"] = priorities;
	info["priorities_autoselect"] = sourceAutoSelectEnabled();

	if (!full)
		return;

	////////////////////////////////////
	// collect adjustment information //
	////////////////////////////////////

	QJsonArray adjustmentArray = _ledColorCalibration->getAdjustmentState();

	info["adjustment"] = adjustmentArray;

	////////////////////////////////////
	//   collect effect information   //
	////////////////////////////////////

	QJsonArray effects;
	std::list<EffectDefinition> effectsDefinitions = getEffects();

	for (const EffectDefinition& effectDefinition : effectsDefinitions)
	{
		QJsonObject effect;
		effect["name"] = QString::fromStdString(effectDefinition.name);
		effects.append(effect);
	}
	info["effects"] = effects;

	////////////////////////////////////
	//    collect running effects     //
	////////////////////////////////////

	QJsonArray activeEffects;
	std::list<ActiveEffectDefinition> activeEffectDefinitionList = _effectEngine->getActiveEffects();

	for (const ActiveEffectDefinition& activeEffectDefinition : activeEffectDefinitionList)
	{
		if (activeEffectDefinition.priority != Muxer::LOWEST_PRIORITY - 1)
		{
			QJsonObject activeEffect;
			activeEffect["name"] = activeEffectDefinition.name;
			activeEffect["priority"] = activeEffectDefinition.priority;
			activeEffect["timeout"] = activeEffectDefinition.timeout;
			activeEffects.append(activeEffect);
		}
	}
	info["activeEffects"] = activeEffects;

	////////////////////////////////////
	//       collect led colors       //
	////////////////////////////////////

	QJsonArray activeLedColors;
	const Muxer::InputInfo& priorityInfo = _muxer->getInputInfo(_muxer->getCurrentPriority());
	if (priorityInfo.componentId == hyperhdr::COMP_COLOR)
	{
		QJsonObject LEDcolor;
		// check if LED Color not Black (0,0,0)
		if ((priorityInfo.staticColor.red +
			priorityInfo.staticColor.green +
			priorityInfo.staticColor.blue !=
			0))
		{
			QJsonObject LEDcolor;

			// add RGB Value to Array
			QJsonArray RGBValue;
			RGBValue.append(priorityInfo.staticColor.red);
			RGBValue.append(priorityInfo.staticColor.green);
			RGBValue.append(priorityInfo.staticColor.blue);
			LEDcolor.insert("RGB Value", RGBValue);

			uint16_t Hue;
			float Saturation, Luminace;

			// add HSL Value to Array
			QJsonArray HSLValue;
			ColorRgb::rgb2hsl(priorityInfo.staticColor.red,
				priorityInfo.staticColor.green,
				priorityInfo.staticColor.blue,
				Hue, Saturation, Luminace);

			HSLValue.append(Hue);
			HSLValue.append(Saturation);
			HSLValue.append(Luminace);
			LEDcolor.insert("HSL Value", HSLValue);

			activeLedColors.append(LEDcolor);
		}
	}
	info["activeLedColor"] = activeLedColors;

	///////////////////////////////////////
	//    collect vailable components    //
	///////////////////////////////////////

	QJsonArray component;
	for (const auto& comp : _componentController->getComponentsState())
	{
		QJsonObject item;
		item["name"] = QString::fromStdString(hyperhdr::componentToIdString(comp.first));
		item["enabled"] = comp.second;

		component.append(item);
	}
	info["components"] = component;

	////////////////
	//    MISC    //
	////////////////

	info["videomodehdr"] = (_componentController->isComponentEnabled(hyperhdr::Components::COMP_HDR)) ? 1 : 0;
	info["leds"] = getSetting(settings::type::LEDS).array();
	info["imageToLedMappingType"] = ImageToLedManager::mappingTypeToStr(_imageProcessor->getLedMappingType());
}

void HyperHdrInstance::setSignalStateByCEC(bool enable)
{
	if (_systemControl != nullptr && _systemControl->isCEC())
	{
		_systemControl->setSysCaptureEnable(enable);
	}
	if (_videoControl != nullptr && _videoControl->isCEC())
	{
		_videoControl->setUsbCaptureEnable(enable);
	}
}

int HyperHdrInstance::hasLedClock()
{
	return _ledDeviceWrapper->hasLedClock();
}

bool HyperHdrInstance::getScanParameters(size_t led, double& hscanBegin, double& hscanEnd, double& vscanBegin, double& vscanEnd) const
{
	return _imageProcessor->getScanParameters(led, hscanBegin, hscanEnd, vscanBegin, vscanEnd);
}


void HyperHdrInstance::turnGrabbers(bool active)
{
	_componentController->turnGrabbers(active);
}
