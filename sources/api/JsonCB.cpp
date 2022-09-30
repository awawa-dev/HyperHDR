// proj incl
#include <api/JsonCB.h>

// hyperhdr
#include <base/HyperHdrInstance.h>

#include <base/GrabberWrapper.h>

// HyperHDRIManager
#include <base/HyperHdrIManager.h>

// components
#include <base/ComponentRegister.h>

// bonjour wrapper
#ifdef ENABLE_BONJOUR
	#include <bonjour/bonjourbrowserwrapper.h>
#endif

// priorityMuxer
#include <base/PriorityMuxer.h>

// utils
#include <utils/ColorSys.h>

#include <utils/PerformanceCounters.h>

#include <utils/LutCalibrator.h>

// qt
#include <QVariant>

// Image to led map helper
#include <base/ImageProcessor.h>

#include <flatbufserver/FlatBufferServer.h>

using namespace hyperhdr;

JsonCB::JsonCB(QObject* parent)
	: QObject(parent)
	, _hyperhdr(nullptr)
	, _componentRegister(nullptr)
#ifdef ENABLE_BONJOUR
	, _bonjour(BonjourBrowserWrapper::getInstance())
#endif
	, _prioMuxer(nullptr)
{
	_availableCommands << "components-update" << "performance-update" << "sessions-update" << "priorities-update" << "imageToLedMapping-update" << "grabberstate-update" << "lut-calibration-update"
		<< "adjustment-update" << "videomodehdr-update" << "effects-update" << "settings-update" << "leds-update" << "instance-update" << "token-update" << "benchmark-update";
}

bool JsonCB::subscribeFor(const QString& type, bool unsubscribe)
{
	if (!_availableCommands.contains(type))
		return false;

	if (unsubscribe)
		_subscribedCommands.removeAll(type);
	else
		_subscribedCommands << type;

	if (type == "components-update")
	{
		if (unsubscribe)
			disconnect(_componentRegister, &ComponentRegister::updatedComponentState, this, &JsonCB::handleComponentState);
		else
			connect(_componentRegister, &ComponentRegister::updatedComponentState, this, &JsonCB::handleComponentState, Qt::UniqueConnection);
	}

	if (type == "performance-update")
	{
		if (unsubscribe)
			disconnect(PerformanceCounters::getInstance(), &PerformanceCounters::performanceUpdate, this, &JsonCB::handlePerformanceUpdate);
		else
		{
			connect(PerformanceCounters::getInstance(), &PerformanceCounters::performanceUpdate, this, &JsonCB::handlePerformanceUpdate, Qt::UniqueConnection);
			emit PerformanceCounters::getInstance()->triggerBroadcast();
		}
	}

	if (type == "lut-calibration-update")
	{
		if (unsubscribe)
			disconnect(LutCalibrator::getInstance(), &LutCalibrator::lutCalibrationUpdate, this, &JsonCB::handleLutCalibrationUpdate);
		else		
			connect(LutCalibrator::getInstance(), &LutCalibrator::lutCalibrationUpdate, this, &JsonCB::handleLutCalibrationUpdate, Qt::UniqueConnection);
	}

	if (type == "sessions-update")
	{
#ifdef ENABLE_BONJOUR
		if (unsubscribe)
			disconnect(_bonjour, &BonjourBrowserWrapper::browserChange, this, &JsonCB::handleBonjourChange);
		else
			connect(_bonjour, &BonjourBrowserWrapper::browserChange, this, &JsonCB::handleBonjourChange, Qt::UniqueConnection);
#endif
	}

	if (type == "priorities-update")
	{
		if (unsubscribe)
			disconnect(_prioMuxer, &PriorityMuxer::prioritiesChanged, this, &JsonCB::handlePriorityUpdate);
		else
			connect(_prioMuxer, &PriorityMuxer::prioritiesChanged, this, &JsonCB::handlePriorityUpdate, Qt::UniqueConnection);
	}

	if (type == "imageToLedMapping-update")
	{
		if (unsubscribe)
			disconnect(_hyperhdr, &HyperHdrInstance::imageToLedsMappingChanged, this, &JsonCB::handleImageToLedsMappingChange);
		else
			connect(_hyperhdr, &HyperHdrInstance::imageToLedsMappingChanged, this, &JsonCB::handleImageToLedsMappingChange, Qt::UniqueConnection);
	}

	if (type == "adjustment-update")
	{
		if (unsubscribe)
			disconnect(_hyperhdr, &HyperHdrInstance::adjustmentChanged, this, &JsonCB::handleAdjustmentChange);
		else
			connect(_hyperhdr, &HyperHdrInstance::adjustmentChanged, this, &JsonCB::handleAdjustmentChange, Qt::UniqueConnection);
	}

	if (type == "grabberstate-update" && GrabberWrapper::instance != nullptr)
	{
		if (unsubscribe)
			disconnect(GrabberWrapper::instance, &GrabberWrapper::StateChanged, this, &JsonCB::handleGrabberStateChange);
		else
			connect(GrabberWrapper::instance, &GrabberWrapper::StateChanged, this, &JsonCB::handleGrabberStateChange, Qt::UniqueConnection);
	}

	if (type == "videomodehdr-update" && GrabberWrapper::instance != nullptr)
	{
		if (unsubscribe)
			disconnect(GrabberWrapper::instance, &GrabberWrapper::HdrChanged, this, &JsonCB::handleVideoModeHdrChange);
		else
			connect(GrabberWrapper::instance, &GrabberWrapper::HdrChanged, this, &JsonCB::handleVideoModeHdrChange, Qt::UniqueConnection);
	}

	if (type == "videomodehdr-update" && GrabberWrapper::instance == nullptr && FlatBufferServer::instance != nullptr)
	{
		if (unsubscribe)
			disconnect(FlatBufferServer::instance, &FlatBufferServer::HdrChanged, this, &JsonCB::handleVideoModeHdrChange);
		else
			connect(FlatBufferServer::instance, &FlatBufferServer::HdrChanged, this, &JsonCB::handleVideoModeHdrChange, Qt::UniqueConnection);
	}

	if (type == "effects-update")
	{
		if (unsubscribe)
			disconnect(_hyperhdr, &HyperHdrInstance::effectListUpdated, this, &JsonCB::handleEffectListChange);
		else
			connect(_hyperhdr, &HyperHdrInstance::effectListUpdated, this, &JsonCB::handleEffectListChange, Qt::UniqueConnection);
	}

	if (type == "settings-update")
	{
		if (unsubscribe)
			disconnect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &JsonCB::handleSettingsChange);
		else
			connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &JsonCB::handleSettingsChange, Qt::UniqueConnection);
	}

	if (type == "leds-update")
	{
		if (unsubscribe)
			disconnect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &JsonCB::handleLedsConfigChange);
		else
			connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &JsonCB::handleLedsConfigChange, Qt::UniqueConnection);
	}


	if (type == "instance-update")
	{
		if (unsubscribe)
			disconnect(HyperHdrIManager::getInstance(), &HyperHdrIManager::change, this, &JsonCB::handleInstanceChange);
		else
			connect(HyperHdrIManager::getInstance(), &HyperHdrIManager::change, this, &JsonCB::handleInstanceChange, Qt::UniqueConnection);
	}

	if (type == "token-update")
	{
		if (unsubscribe)
			disconnect(AuthManager::getInstance(), &AuthManager::tokenChange, this, &JsonCB::handleTokenChange);
		else
			connect(AuthManager::getInstance(), &AuthManager::tokenChange, this, &JsonCB::handleTokenChange, Qt::UniqueConnection);
	}

	if (type == "benchmark-update" && GrabberWrapper::instance != nullptr)
	{
		if (unsubscribe)
			disconnect(GrabberWrapper::instance, &GrabberWrapper::benchmarkUpdate, this, &JsonCB::handleBenchmarkUpdate);
		else
			connect(GrabberWrapper::instance, &GrabberWrapper::benchmarkUpdate, this, &JsonCB::handleBenchmarkUpdate, Qt::UniqueConnection);
	}

	return true;
}

QStringList JsonCB::getCommands()
{
	return _availableCommands;
};

QStringList JsonCB::getSubscribedCommands()
{
	return _subscribedCommands;
};

void JsonCB::resetSubscriptions()
{
	for (const auto& entry : getSubscribedCommands())
	{
		subscribeFor(entry, true);
	}
}

void JsonCB::setSubscriptionsTo(HyperHdrInstance* hyperhdr)
{
	// get current subs
	QStringList currSubs(getSubscribedCommands());

	// stop subs
	resetSubscriptions();

	// update pointer
	_hyperhdr = hyperhdr;
	_componentRegister = &_hyperhdr->getComponentRegister();
	_prioMuxer = _hyperhdr->getMuxerInstance();

	// re-apply subs
	for (const auto& entry : currSubs)
	{
		subscribeFor(entry);
	}
}

void JsonCB::doCallback(const QString& cmd, const QVariant& data)
{
	QJsonObject obj;
	obj["command"] = cmd;

	if (data.userType() == QMetaType::QJsonArray)
		obj["data"] = data.toJsonArray();
	else
		obj["data"] = data.toJsonObject();

	emit newCallback(obj);
}

void JsonCB::handleComponentState(hyperhdr::Components comp, bool state)
{
	QJsonObject data;
	data["name"] = componentToIdString(comp);
	data["enabled"] = state;

	doCallback("components-update", QVariant(data));
}
#ifdef ENABLE_BONJOUR
void JsonCB::handleBonjourChange(const QMap<QString, BonjourRecord>& bRegisters)
{
	QJsonArray data;
	for (auto&& session : bRegisters)
	{
		if (session.port < 0) continue;
		QJsonObject item;
		item["name"] = session.serviceName;
		item["type"] = session.registeredType;
		item["domain"] = session.replyDomain;
		item["host"] = session.hostName;
		item["address"] = session.address;
		item["port"] = session.port;
		data.append(item);
	}

	doCallback("sessions-update", QVariant(data));
}
#endif
void JsonCB::handlePriorityUpdate()
{
	QJsonObject data;
	QJsonArray priorities;
	int64_t now = InternalClock::now();
	QList<int> activePriorities = _prioMuxer->getPriorities();
	activePriorities.removeAll(255);
	int currentPriority = _prioMuxer->getCurrentPriority();

	for (int priority : activePriorities) {
		const HyperHdrInstance::InputInfo priorityInfo = _prioMuxer->getInputInfo(priority);
		QJsonObject item;
		item["priority"] = priority;
		if (priorityInfo.timeoutTime_ms > 0)
			item["duration_ms"] = int(priorityInfo.timeoutTime_ms - now);

		// owner has optional informations to the component
		if (!priorityInfo.owner.isEmpty())
			item["owner"] = priorityInfo.owner;

		item["componentId"] = QString(hyperhdr::componentToIdString(priorityInfo.componentId));
		item["origin"] = priorityInfo.origin;
		item["active"] = (priorityInfo.timeoutTime_ms >= -1);
		item["visible"] = (priority == currentPriority);

		if (priorityInfo.componentId == hyperhdr::COMP_COLOR && !priorityInfo.ledColors.empty())
		{
			QJsonObject LEDcolor;

			// add RGB Value to Array
			QJsonArray RGBValue;
			RGBValue.append(priorityInfo.ledColors.begin()->red);
			RGBValue.append(priorityInfo.ledColors.begin()->green);
			RGBValue.append(priorityInfo.ledColors.begin()->blue);
			LEDcolor.insert("RGB", RGBValue);

			uint16_t Hue;
			float Saturation, Luminace;

			// add HSL Value to Array
			QJsonArray HSLValue;
			ColorSys::rgb2hsl(priorityInfo.ledColors.begin()->red,
				priorityInfo.ledColors.begin()->green,
				priorityInfo.ledColors.begin()->blue,
				Hue, Saturation, Luminace);

			HSLValue.append(Hue);
			HSLValue.append(Saturation);
			HSLValue.append(Luminace);
			LEDcolor.insert("HSL", HSLValue);

			item["value"] = LEDcolor;
		}
		priorities.append(item);
	}

	data["priorities"] = priorities;
	data["priorities_autoselect"] = _hyperhdr->sourceAutoSelectEnabled();

	doCallback("priorities-update", QVariant(data));
}

void JsonCB::handleImageToLedsMappingChange(int mappingType)
{
	QJsonObject data;
	data["imageToLedMappingType"] = ImageProcessor::mappingTypeToStr(mappingType);

	doCallback("imageToLedMapping-update", QVariant(data));
}

void JsonCB::handleAdjustmentChange()
{
	QJsonArray adjustmentArray;
	for (const QString& adjustmentId : _hyperhdr->getAdjustmentIds())
	{
		const ColorAdjustment* colorAdjustment = _hyperhdr->getAdjustment(adjustmentId);
		if (colorAdjustment == nullptr)
		{
			continue;
		}

		QJsonObject adjustment;
		adjustment["id"] = adjustmentId;

		QJsonArray whiteAdjust;
		whiteAdjust.append(colorAdjustment->_rgbWhiteAdjustment.getAdjustmentR());
		whiteAdjust.append(colorAdjustment->_rgbWhiteAdjustment.getAdjustmentG());
		whiteAdjust.append(colorAdjustment->_rgbWhiteAdjustment.getAdjustmentB());
		adjustment.insert("white", whiteAdjust);

		QJsonArray redAdjust;
		redAdjust.append(colorAdjustment->_rgbRedAdjustment.getAdjustmentR());
		redAdjust.append(colorAdjustment->_rgbRedAdjustment.getAdjustmentG());
		redAdjust.append(colorAdjustment->_rgbRedAdjustment.getAdjustmentB());
		adjustment.insert("red", redAdjust);

		QJsonArray greenAdjust;
		greenAdjust.append(colorAdjustment->_rgbGreenAdjustment.getAdjustmentR());
		greenAdjust.append(colorAdjustment->_rgbGreenAdjustment.getAdjustmentG());
		greenAdjust.append(colorAdjustment->_rgbGreenAdjustment.getAdjustmentB());
		adjustment.insert("green", greenAdjust);

		QJsonArray blueAdjust;
		blueAdjust.append(colorAdjustment->_rgbBlueAdjustment.getAdjustmentR());
		blueAdjust.append(colorAdjustment->_rgbBlueAdjustment.getAdjustmentG());
		blueAdjust.append(colorAdjustment->_rgbBlueAdjustment.getAdjustmentB());
		adjustment.insert("blue", blueAdjust);

		QJsonArray cyanAdjust;
		cyanAdjust.append(colorAdjustment->_rgbCyanAdjustment.getAdjustmentR());
		cyanAdjust.append(colorAdjustment->_rgbCyanAdjustment.getAdjustmentG());
		cyanAdjust.append(colorAdjustment->_rgbCyanAdjustment.getAdjustmentB());
		adjustment.insert("cyan", cyanAdjust);

		QJsonArray magentaAdjust;
		magentaAdjust.append(colorAdjustment->_rgbMagentaAdjustment.getAdjustmentR());
		magentaAdjust.append(colorAdjustment->_rgbMagentaAdjustment.getAdjustmentG());
		magentaAdjust.append(colorAdjustment->_rgbMagentaAdjustment.getAdjustmentB());
		adjustment.insert("magenta", magentaAdjust);

		QJsonArray yellowAdjust;
		yellowAdjust.append(colorAdjustment->_rgbYellowAdjustment.getAdjustmentR());
		yellowAdjust.append(colorAdjustment->_rgbYellowAdjustment.getAdjustmentG());
		yellowAdjust.append(colorAdjustment->_rgbYellowAdjustment.getAdjustmentB());
		adjustment.insert("yellow", yellowAdjust);

		adjustment["backlightThreshold"] = colorAdjustment->_rgbTransform.getBacklightThreshold();
		adjustment["backlightColored"] = colorAdjustment->_rgbTransform.getBacklightColored();
		adjustment["brightness"] = colorAdjustment->_rgbTransform.getBrightness();
		adjustment["brightnessCompensation"] = colorAdjustment->_rgbTransform.getBrightnessCompensation();
		adjustment["gammaRed"] = colorAdjustment->_rgbTransform.getGammaR();
		adjustment["gammaGreen"] = colorAdjustment->_rgbTransform.getGammaG();
		adjustment["gammaBlue"] = colorAdjustment->_rgbTransform.getGammaB();

		adjustmentArray.append(adjustment);
	}

	doCallback("adjustment-update", QVariant(adjustmentArray));
}

void JsonCB::handleVideoModeHdrChange(int hdr)
{
	QJsonObject data;
	data["videomodehdr"] = hdr;
	doCallback("videomodehdr-update", QVariant(data));
}

void JsonCB::handleGrabberStateChange(QString device, QString videoMode)
{
	QJsonObject data;
	data["device"] = device;
	data["videoMode"] = videoMode;
	doCallback("grabberstate-update", QVariant(data));
}

void JsonCB::handleEffectListChange()
{
	QJsonArray effectList;
	QJsonObject effects;
	const std::list<EffectDefinition>& effectsDefinitions = _hyperhdr->getEffects();
	for (const EffectDefinition& effectDefinition : effectsDefinitions)
	{
		QJsonObject effect;
		effect["name"] = effectDefinition.name;
		effect["args"] = effectDefinition.args;
		effectList.append(effect);
	};
	effects["effects"] = effectList;
	doCallback("effects-update", QVariant(effects));
}

void JsonCB::handleSettingsChange(settings::type type, const QJsonDocument& data)
{
	QJsonObject dat;
	if (data.isObject())
		dat[typeToString(type)] = data.object();
	else
		dat[typeToString(type)] = data.array();

	doCallback("settings-update", QVariant(dat));
}

void JsonCB::handleLedsConfigChange(settings::type type, const QJsonDocument& data)
{
	if (type == settings::type::LEDS)
	{
		QJsonObject dat;
		dat[typeToString(type)] = data.array();
		doCallback("leds-update", QVariant(dat));
	}
}

void JsonCB::handleInstanceChange()
{
	QJsonArray arr;

	for (const auto& entry : HyperHdrIManager::getInstance()->getInstanceData())
	{
		QJsonObject obj;
		obj.insert("friendly_name", entry["friendly_name"].toString());
		obj.insert("instance", entry["instance"].toInt());
		//obj.insert("last_use", entry["last_use"].toString());
		obj.insert("running", entry["running"].toBool());
		arr.append(obj);
	}
	doCallback("instance-update", QVariant(arr));
}

void JsonCB::handleTokenChange(const QVector<AuthManager::AuthDefinition>& def)
{
	QJsonArray arr;
	for (const auto& entry : def)
	{
		QJsonObject sub;
		sub["comment"] = entry.comment;
		sub["id"] = entry.id;
		sub["last_use"] = entry.lastUse;
		arr.push_back(sub);
	}
	doCallback("token-update", QVariant(arr));
}

void JsonCB::handleBenchmarkUpdate(int status, QString message)
{
	QJsonObject dat;
	dat["status"] = status;
	dat["message"] = message;
	doCallback("benchmark-update", QVariant(dat));
}

void JsonCB::handleLutCalibrationUpdate(const QJsonObject& data)
{	
	doCallback("lut-calibration-update", QVariant(data));
}


void JsonCB::handlePerformanceUpdate(const QJsonObject& data)
{
	doCallback("performance-update", QVariant(data));	
}
