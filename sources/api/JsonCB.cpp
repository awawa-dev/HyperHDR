#include <HyperhdrConfig.h>

#include <api/JsonCB.h>
#include <base/HyperHdrInstance.h>
#include <base/GrabberWrapper.h>
#include <base/HyperHdrIManager.h>
#include <base/ComponentRegister.h>
#include <base/PriorityMuxer.h>
#include <base/ImageProcessor.h>
#include <utils/ColorSys.h>
#include <utils/PerformanceCounters.h>
#include <utils/LutCalibrator.h>
#include <flatbufserver/FlatBufferServer.h>

#include <QVariant>

using namespace hyperhdr;

JsonCB::JsonCB(QObject* parent)
	: QObject(parent)
	, _hyperhdr(nullptr)
{
	_availableCommands << "components-update" << "performance-update" << "sessions-update" << "priorities-update" << "imageToLedMapping-update" << "grabberstate-update" << "lut-calibration-update"
		<< "adjustment-update" << "videomodehdr-update" << "settings-update" << "leds-update" << "instance-update" << "token-update" << "benchmark-update";
}

void JsonCB::setSubscriptionsTo(HyperHdrInstance* hyperhdr)
{
	// get current subs
	QStringList currSubs(getSubscribedCommands());

	if (hyperhdr == nullptr)
	{
		_hyperhdr = nullptr;
		return;
	}

	// stop subs
	resetSubscriptions();

	// update pointer
	_hyperhdr = hyperhdr;

	// re-apply subs
	for (const auto& entry : currSubs)
	{
		subscribeFor(entry);
	}
}

bool JsonCB::subscribeFor(const QString& type, bool unsubscribe)
{
	if (!_availableCommands.contains(type))
		return false;

	if (unsubscribe)
		_subscribedCommands.removeAll(type);
	else
		_subscribedCommands << type;

	if (type == "components-update" && _hyperhdr != nullptr)
	{
		ComponentRegister* _componentRegister = &_hyperhdr->getComponentRegister();
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
			disconnect(DiscoveryWrapper::getInstance(), &DiscoveryWrapper::foundService, this, &JsonCB::handleNetworkDiscoveryChange);
		else
			connect(DiscoveryWrapper::getInstance(), &DiscoveryWrapper::foundService, this, &JsonCB::handleNetworkDiscoveryChange, Qt::UniqueConnection);
#endif
	}

	if (type == "priorities-update" && _hyperhdr != nullptr)
	{
		PriorityMuxer* _prioMuxer = _hyperhdr->getMuxerInstance();
		if (unsubscribe)
			disconnect(_prioMuxer, &PriorityMuxer::prioritiesChanged, this, &JsonCB::handlePriorityUpdate);
		else
			connect(_prioMuxer, &PriorityMuxer::prioritiesChanged, this, &JsonCB::handlePriorityUpdate, Qt::UniqueConnection);
	}

	if (type == "imageToLedMapping-update" && _hyperhdr != nullptr)
	{
		if (unsubscribe)
			disconnect(_hyperhdr, &HyperHdrInstance::imageToLedsMappingChanged, this, &JsonCB::handleImageToLedsMappingChange);
		else
			connect(_hyperhdr, &HyperHdrInstance::imageToLedsMappingChanged, this, &JsonCB::handleImageToLedsMappingChange, Qt::UniqueConnection);
	}

	if (type == "adjustment-update" && _hyperhdr != nullptr)
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

	if (type == "settings-update" && _hyperhdr != nullptr)
	{
		if (unsubscribe)
			disconnect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &JsonCB::handleSettingsChange);
		else
			connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &JsonCB::handleSettingsChange, Qt::UniqueConnection);
	}

	if (type == "leds-update" && _hyperhdr != nullptr)
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
	void JsonCB::handleNetworkDiscoveryChange(DiscoveryRecord::Service type, QList<DiscoveryRecord> records)
	{
		QJsonObject retValue;
		QJsonArray data;

		for (const auto& session : records)
		{
			QJsonObject item;
			item["name"] = session.getName();
			item["host"] = session.hostName;
			item["address"] = session.address;
			item["port"] = session.port;
			data.append(item);
		}

		retValue["service"] = DiscoveryRecord::getName(type);
		retValue["items"] = data;
		doCallback("sessions-update", QVariant(retValue));
	}
#endif

void JsonCB::handlePriorityUpdate()
{
	QJsonObject info;
	QJsonArray priorities;

	if (_hyperhdr == nullptr)
		return;

	SAFE_CALL_1_RET(_hyperhdr, getJsonInfo, QJsonObject, info, bool, false);

	doCallback("priorities-update", QVariant(info));
}

void JsonCB::handleImageToLedsMappingChange(int mappingType)
{
	QJsonObject data;
	data["imageToLedMappingType"] = ImageProcessor::mappingTypeToStr(mappingType);

	doCallback("imageToLedMapping-update", QVariant(data));
}

void JsonCB::handleAdjustmentChange()
{
	if (_hyperhdr == nullptr)
		return;

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

void JsonCB::handleLutInstallUpdate(const QJsonObject& data)
{
	doCallback("lut-install-update", QVariant(data));
}
