// project includes
#include <api/JsonAPI.h>

// stl includes
#include <chrono>
#include <csignal>

// Qt includes
#include <QCoreApplication>
#include <QResource>
#include <QDateTime>
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QTimer>
#include <QHostInfo>
#include <QMultiMap>
#include <QSerialPortInfo>

#include <leddevice/LedDeviceWrapper.h>
#include <leddevice/LedDevice.h>
#include <leddevice/LedDeviceFactory.h>

#include <base/GrabberWrapper.h>
#include <base/SystemWrapper.h>
#include <base/SoundCapture.h>
#include <utils/jsonschema/QJsonUtils.h>
#include <utils/jsonschema/QJsonSchemaChecker.h>
#include <HyperhdrConfig.h>
#include <utils/SysInfo.h>
#include <utils/ColorSys.h>
#include <utils/JsonUtils.h>
#include <utils/PerformanceCounters.h>
#include <utils/LutCalibrator.h>

// bonjour wrapper
#ifdef ENABLE_AVAHI
#include <bonjour/bonjourbrowserwrapper.h>
#endif

// ledmapping int <> string transform methods
#include <base/ImageProcessor.h>

// api includes
#include <api/JsonCB.h>

// auth manager
#include <base/AuthManager.h>

#include <flatbufserver/FlatBufferServer.h>

using namespace hyperhdr;

JsonAPI::JsonAPI(QString peerAddress, Logger* log, bool localConnection, QObject* parent, bool noListener)
	: API(log, localConnection, parent), _semaphore(1)
{
	_noListener = noListener;
	_peerAddress = peerAddress;
	_jsonCB = new JsonCB(this);
	_streaming_logging_activated = false;
	_ledStreamTimer = new QTimer(this);
	_lastSendImage = QDateTime::currentMSecsSinceEpoch();

	Q_INIT_RESOURCE(JSONRPC_schemas);
}

void JsonAPI::initialize()
{
	// init API, REQUIRED!
	API::init();
	// REMOVE when jsonCB is migrated
	handleInstanceSwitch(0);

	// setup auth interface
	connect(this, &API::onPendingTokenRequest, this, &JsonAPI::newPendingTokenRequest);
	connect(this, &API::onTokenResponse, this, &JsonAPI::handleTokenResponse);

	// listen for killed instances
	connect(_instanceManager, &HyperHdrIManager::instanceStateChanged, this, &JsonAPI::handleInstanceStateChange);

	// pipe callbacks from subscriptions to parent
	connect(_jsonCB, &JsonCB::newCallback, this, &JsonAPI::callbackMessage);

	// notify hyperhdr about a jsonMessageForward
	if (_hyperhdr != nullptr)
		connect(this, &JsonAPI::forwardJsonMessage, _hyperhdr, &HyperHdrInstance::forwardJsonMessage);
}

bool JsonAPI::handleInstanceSwitch(quint8 inst, bool forced)
{
	if (API::setHyperhdrInstance(inst))
	{
		Debug(_log, "Client '%s' switch to HyperHDR instance %d", QSTRING_CSTR(_peerAddress), inst);
		// the JsonCB creates json messages you can subscribe to e.g. data change events
		_jsonCB->setSubscriptionsTo(_hyperhdr);
		return true;
	}
	return false;
}

void JsonAPI::handleMessage(const QString& messageString, const QString& httpAuthHeader)
{
	try
	{
		const QString ident = "JsonRpc@" + _peerAddress;
		QJsonObject message;
		// parse the message
		if (!JsonUtils::parse(ident, messageString, message, _log))
		{
			sendErrorReply("Errors during message parsing, please consult the HyperHDR Log.");
			return;
		}

		int tan = 0;
		if (message.value("tan") != QJsonValue::Undefined)
			tan = message["tan"].toInt();

		// check basic message
		if (!JsonUtils::validate(ident, message, ":schema", _log))
		{
			sendErrorReply("Errors during message validation, please consult the HyperHDR Log.", "" /*command*/, tan);
			return;
		}

		// check specific message
		const QString command = message["command"].toString();
		if (!JsonUtils::validate(ident, message, QString(":schema-%1").arg(command), _log))
		{
			sendErrorReply("Errors during specific message validation, please consult the HyperHDR Log", command, tan);
			return;
		}

		// client auth before everything else but not for http
		if (!_noListener && command == "authorize")
		{
			handleAuthorizeCommand(message, command, tan);
			return;
		}

		// check auth state
		if (!API::isAuthorized())
		{
			// on the fly auth available for http from http Auth header
			if (_noListener)
			{
				QString cToken = httpAuthHeader.mid(5).trimmed();
				if (API::isTokenAuthorized(cToken))
					goto proceed;
			}
			sendErrorReply("No Authorization", command, tan);
			return;
		}
	proceed:
		if (_hyperhdr == nullptr)
		{
			sendErrorReply("Not ready", command, tan);
			return;
		}

		// switch over all possible commands and handle them
		if (command == "color")
			handleColorCommand(message, command, tan);
		else if (command == "image")
			handleImageCommand(message, command, tan);
		else if (command == "effect")
			handleEffectCommand(message, command, tan);
		else if (command == "create-effect")
			handleCreateEffectCommand(message, command, tan);
		else if (command == "delete-effect")
			handleDeleteEffectCommand(message, command, tan);
		else if (command == "sysinfo")
			handleSysInfoCommand(message, command, tan);
		else if (command == "serverinfo")
			handleServerInfoCommand(message, command, tan);
		else if (command == "clear")
			handleClearCommand(message, command, tan);
		else if (command == "adjustment")
			handleAdjustmentCommand(message, command, tan);
		else if (command == "sourceselect")
			handleSourceSelectCommand(message, command, tan);
		else if (command == "config")
			handleConfigCommand(message, command, tan);
		else if (command == "componentstate")
			handleComponentStateCommand(message, command, tan);
		else if (command == "ledcolors")
			handleLedColorsCommand(message, command, tan);
		else if (command == "logging")
			handleLoggingCommand(message, command, tan);
		else if (command == "processing")
			handleProcessingCommand(message, command, tan);
		else if (command == "videomodehdr")
			handleVideoModeHdrCommand(message, command, tan);
		else if (command == "lut-calibration")
			handleLutCalibrationCommand(message, command, tan);
		else if (command == "instance")
			handleInstanceCommand(message, command, tan);
		else if (command == "leddevice")
			handleLedDeviceCommand(message, command, tan);
		else if (command == "save-db")
			handleSaveDB(message, command, tan);
		else if (command == "load-db")
			handleLoadDB(message, command, tan);
		else if (command == "signal-calibration")
			handleLoadSignalCalibration(message, command, tan);
		else if (command == "performance-counters")
			handlePerformanceCounters(message, command, tan);
		else if (command == "clearall")
			handleClearallCommand(message, command, tan);
		else if (command == "help")
			handleHelpCommand(message, command, tan);
		else if (command == "video-crop")
			handleCropCommand(message, command, tan);
		else if (command == "video-controls")
			handleVideoControlsCommand(message, command, tan);
		else if (command == "benchmark")
			handleBenchmarkCommand(message, command, tan);
		else if (command == "transform" || command == "correction" || command == "temperature")
			sendErrorReply("The command " + command + "is deprecated, please use the HyperHDR Web Interface to configure", command, tan);
		// END

		// handle not implemented commands
		else
			handleNotImplemented(command, tan);
	}
	catch(...)
	{
		sendErrorReply("Exception");
	}
}

void JsonAPI::handleColorCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit forwardJsonMessage(message);
	int priority = message["priority"].toInt();
	int duration = message["duration"].toInt(-1);
	const QString origin = message["origin"].toString("JsonRpc") + "@" + _peerAddress;

	const QJsonArray& jsonColor = message["color"].toArray();
	std::vector<uint8_t> colors;
	// TODO faster copy
	for (auto&& entry : jsonColor)
	{
		colors.emplace_back(uint8_t(entry.toInt()));
	}

	API::setColor(priority, colors, duration, origin);
	sendSuccessReply(command, tan);
}

void JsonAPI::handleImageCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit forwardJsonMessage(message);

	API::ImageCmdData idata;
	idata.priority = message["priority"].toInt();
	idata.origin = message["origin"].toString("JsonRpc") + "@" + _peerAddress;
	idata.duration = message["duration"].toInt(-1);
	idata.width = message["imagewidth"].toInt();
	idata.height = message["imageheight"].toInt();
	idata.scale = message["scale"].toInt(-1);
	idata.format = message["format"].toString();
	idata.imgName = message["name"].toString("");
	idata.data = QByteArray::fromBase64(QByteArray(message["imagedata"].toString().toUtf8()));
	QString replyMsg;

	if (!API::setImage(idata, COMP_IMAGE, replyMsg))
	{
		sendErrorReply(replyMsg, command, tan);
		return;
	}
	sendSuccessReply(command, tan);
}

void JsonAPI::handleEffectCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit forwardJsonMessage(message);

	EffectCmdData dat;
	dat.priority = message["priority"].toInt();
	dat.duration = message["duration"].toInt(-1);
	dat.pythonScript = message["pythonScript"].toString();
	dat.origin = message["origin"].toString("JsonRpc") + "@" + _peerAddress;
	dat.effectName = message["effect"].toObject()["name"].toString();
	dat.data = message["imageData"].toString("").toUtf8();
	dat.args = message["effect"].toObject()["args"].toObject();

	if (API::setEffect(dat))
		sendSuccessReply(command, tan);
	else
		sendErrorReply("Effect '" + dat.effectName + "' not found", command, tan);
}

void JsonAPI::handleCreateEffectCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString resultMsg = API::saveEffect(message);
	resultMsg.isEmpty() ? sendSuccessReply(command, tan) : sendErrorReply(resultMsg, command, tan);
}

void JsonAPI::handleDeleteEffectCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString res = API::deleteEffect(message["name"].toString());
	res.isEmpty() ? sendSuccessReply(command, tan) : sendErrorReply(res, command, tan);
}

void JsonAPI::handleSysInfoCommand(const QJsonObject&, const QString& command, int tan)
{
	// create result
	QJsonObject result;
	QJsonObject info;
	result["success"] = true;
	result["command"] = command;
	result["tan"] = tan;

	SysInfo::HyperhdrSysInfo data = SysInfo::get();
	QJsonObject system;
	system["kernelType"] = data.kernelType;
	system["kernelVersion"] = data.kernelVersion;
	system["architecture"] = data.architecture;
	system["cpuModelName"] = data.cpuModelName;
	system["cpuModelType"] = data.cpuModelType;
	system["cpuHardware"] = data.cpuHardware;
	system["cpuRevision"] = data.cpuRevision;
	system["wordSize"] = data.wordSize;
	system["productType"] = data.productType;
	system["productVersion"] = data.productVersion;
	system["prettyName"] = data.prettyName;
	system["hostName"] = data.hostName;
	system["domainName"] = data.domainName;
	system["qtVersion"] = data.qtVersion;
	system["pyVersion"] = data.pyVersion;
	info["system"] = system;

	QJsonObject hyperhdr;
	hyperhdr["version"] = QString(HYPERHDR_VERSION);
	hyperhdr["build"] = QString(HYPERHDR_BUILD_ID);
	hyperhdr["gitremote"] = QString(HYPERHDR_GIT_REMOTE);
	hyperhdr["time"] = QString(__DATE__ " " __TIME__);
	hyperhdr["id"] = _authManager->getID();

	bool readOnly = true;
	if (QThread::currentThread() == _hyperhdr->thread())
		readOnly = _hyperhdr->getReadOnlyMode();
	else
		QMetaObject::invokeMethod(_hyperhdr, "getReadOnlyMode", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(bool, readOnly));
	hyperhdr["readOnlyMode"] = readOnly;

	info["hyperhdr"] = hyperhdr;

	// send the result
	result["info"] = info;
	emit callbackMessage(result);
}

void JsonAPI::handleServerInfoCommand(const QJsonObject& message, const QString& command, int tan)
{
	try
	{
		QJsonObject info;

		// collect priority information
		QJsonArray priorities;
		uint64_t now = QDateTime::currentMSecsSinceEpoch();

		int currentPriority = -1;
		if (QThread::currentThread() == _hyperhdr->thread())
			currentPriority = _hyperhdr->getCurrentPriority();
		else
			QMetaObject::invokeMethod(_hyperhdr, "getCurrentPriority", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(int, currentPriority));

		QList<int> activePriorities = _hyperhdr->getActivePriorities();
		activePriorities.removeAll(255);
		

		for (int priority : activePriorities)
		{
			const HyperHdrInstance::InputInfo& priorityInfo = _hyperhdr->getPriorityInfo(priority);
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


			(priority == currentPriority)
				? priorities.prepend(item)
				: priorities.append(item);
		}

		info["priorities"] = priorities;
		info["priorities_autoselect"] = _hyperhdr->sourceAutoSelectEnabled();

		// collect adjustment information
		QJsonArray adjustmentArray;
		for (const QString& adjustmentId : _hyperhdr->getAdjustmentIds())
		{
			const ColorAdjustment* colorAdjustment = _hyperhdr->getAdjustment(adjustmentId);
			if (colorAdjustment == nullptr)
			{
				Error(_log, "Incorrect color adjustment id: %s", QSTRING_CSTR(adjustmentId));
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
			adjustment["temperatureRed"] = colorAdjustment->_rgbRedAdjustment.getCorrection();
			adjustment["temperatureGreen"] = colorAdjustment->_rgbGreenAdjustment.getCorrection();
			adjustment["temperatureBlue"] = colorAdjustment->_rgbBlueAdjustment.getCorrection();
			adjustment["saturationGain"] = colorAdjustment->_rgbTransform.getSaturationGain();
			adjustment["luminanceGain"] = colorAdjustment->_rgbTransform.getLuminanceGain();
			adjustment["classic_config"] = colorAdjustment->_rgbTransform.getClassicConfig();

			adjustmentArray.append(adjustment);
		}

		info["adjustment"] = adjustmentArray;

		// collect effect info
		QJsonArray effects;
		const std::list<EffectDefinition>& effectsDefinitions = _hyperhdr->getEffects();
		for (const EffectDefinition& effectDefinition : effectsDefinitions)
		{
			QJsonObject effect;
			effect["name"] = effectDefinition.name;
			effect["args"] = effectDefinition.args;
			effects.append(effect);
		}

		info["effects"] = effects;

		// get available led devices
		QJsonObject ledDevices;
		QJsonArray availableLedDevices;
		for (auto dev : LedDeviceWrapper::getDeviceMap())
		{
			availableLedDevices.append(dev.first);
		}

		ledDevices["available"] = availableLedDevices;
		info["ledDevices"] = ledDevices;

		// serial port
		QJsonArray availableSerialPorts;
		const auto infoSerial = QSerialPortInfo::availablePorts();
		for (const QSerialPortInfo& info : infoSerial) {
			QJsonObject serialPort;
			serialPort["port"] = info.portName();
			serialPort["desc"] = QString("%2 (%1)").arg(info.systemLocation()).arg(info.description());
			availableSerialPorts.append(serialPort);
		}
		info["serialPorts"] = availableSerialPorts;


	#if defined(ENABLE_SOUNDCAPLINUX) || defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPMACOS)
		if (SoundCapture::getInstance() != NULL)
		{
			QJsonObject resultSound;
			auto soundInstance = SoundCapture::getInstance();

			if (QThread::currentThread() == soundInstance->thread())
				resultSound = soundInstance->getJsonInfo();
			else
				QMetaObject::invokeMethod(soundInstance, "getJsonInfo", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(QJsonObject, resultSound));

			info["sound"] = resultSound;
		}
	#endif	

	#if defined(ENABLE_DX) || defined(ENABLE_MAC_SYSTEM) || defined(ENABLE_X11) || defined(ENABLE_FRAMEBUFFER)
		if (SystemWrapper::getInstance() != nullptr)
		{
			info["systemGrabbers"] = SystemWrapper::getInstance()->getJsonInfo();
		}
	#endif

	#if defined(ENABLE_PROTOBUF)
		info["hasPROTOBUF"] = 1;
	#else
		info["hasPROTOBUF"] = 0;
	#endif

	#if defined(ENABLE_CEC)
		info["hasCEC"] = 1;
	#else
		info["hasCEC"] = 0;
	#endif

		QJsonObject grabbers;
	#if defined(ENABLE_V4L2) || defined(ENABLE_MF) || defined(ENABLE_AVF)	
		grabbers = GrabberWrapper::getInstance()->getJsonInfo();
	#endif
		info["grabbers"] = grabbers;

		if (GrabberWrapper::getInstance() != nullptr)
		{
			info["videomodehdr"] = GrabberWrapper::getInstance()->getHdrToneMappingEnabled();
		}
		else
		{
			if (FlatBufferServer::getInstance() != nullptr)
				info["videomodehdr"] = FlatBufferServer::getInstance()->getHdrToneMappingEnabled();
			else
				info["videomodehdr"] = 0;
		}

		// get available components
		QJsonArray component;
		std::map<hyperhdr::Components, bool> components = _hyperhdr->getComponentRegister().getRegister();
		for (auto comp : components)
		{
			QJsonObject item;
			item["name"] = QString::fromStdString(hyperhdr::componentToIdString(comp.first));
			item["enabled"] = comp.second;

			component.append(item);
		}

		info["components"] = component;
		info["imageToLedMappingType"] = ImageProcessor::mappingTypeToStr(_hyperhdr->getLedMappingType());

		// add sessions
		QJsonArray sessions;
	#ifdef ENABLE_AVAHI
		for (auto session : BonjourBrowserWrapper::getInstance()->getAllServices())
		{
			if (session.port < 0)
				continue;
			QJsonObject item;
			item["name"] = session.serviceName;
			item["type"] = session.registeredType;
			item["domain"] = session.replyDomain;
			item["host"] = session.hostName;
			item["address"] = session.address;
			item["port"] = session.port;
			sessions.append(item);
		}
		info["sessions"] = sessions;
	#endif
		// add instance info
		QJsonArray instanceInfo;
		for (const auto& entry : API::getAllInstanceData())
		{
			QJsonObject obj;
			obj.insert("friendly_name", entry["friendly_name"].toString());
			obj.insert("instance", entry["instance"].toInt());
			//obj.insert("last_use", entry["last_use"].toString());
			obj.insert("running", entry["running"].toBool());
			instanceInfo.append(obj);
		}
		info["instance"] = instanceInfo;

		// add leds configs
		info["leds"] = _hyperhdr->getSetting(settings::type::LEDS).array();

		// HOST NAME
		info["hostname"] = QHostInfo::localHostName();

		// TRANSFORM INFORMATION (DEFAULT VALUES)
		QJsonArray transformArray;
		for (const QString& transformId : _hyperhdr->getAdjustmentIds())
		{
			QJsonObject transform;
			QJsonArray blacklevel, whitelevel, gamma, threshold;

			transform["id"] = transformId;
			transform["saturationGain"] = 1.0;
			transform["valueGain"] = 1.0;
			transform["saturationLGain"] = 1.0;
			transform["luminanceGain"] = 1.0;
			transform["luminanceMinimum"] = 0.0;

			for (int i = 0; i < 3; i++)
			{
				blacklevel.append(0.0);
				whitelevel.append(1.0);
				gamma.append(2.50);
				threshold.append(0.0);
			}

			transform.insert("blacklevel", blacklevel);
			transform.insert("whitelevel", whitelevel);
			transform.insert("gamma", gamma);
			transform.insert("threshold", threshold);

			transformArray.append(transform);
		}
		info["transform"] = transformArray;

		// ACTIVE EFFECT INFO
		QJsonArray activeEffects;
		for (const ActiveEffectDefinition& activeEffectDefinition : _hyperhdr->getActiveEffects())
		{
			if (activeEffectDefinition.priority != PriorityMuxer::LOWEST_PRIORITY - 1)
			{
				QJsonObject activeEffect;
				activeEffect["name"] = activeEffectDefinition.name;
				activeEffect["priority"] = activeEffectDefinition.priority;
				activeEffect["timeout"] = activeEffectDefinition.timeout;
				activeEffect["args"] = activeEffectDefinition.args;
				activeEffects.append(activeEffect);
			}
		}
		info["activeEffects"] = activeEffects;

		// ACTIVE STATIC LED COLOR
		QJsonArray activeLedColors;
		const HyperHdrInstance::InputInfo& priorityInfo = _hyperhdr->getPriorityInfo(_hyperhdr->getCurrentPriority());
		if (priorityInfo.componentId == hyperhdr::COMP_COLOR && !priorityInfo.ledColors.empty())
		{
			QJsonObject LEDcolor;
			// check if LED Color not Black (0,0,0)
			if ((priorityInfo.ledColors.begin()->red +
				priorityInfo.ledColors.begin()->green +
				priorityInfo.ledColors.begin()->blue !=
				0))
			{
				QJsonObject LEDcolor;

				// add RGB Value to Array
				QJsonArray RGBValue;
				RGBValue.append(priorityInfo.ledColors.begin()->red);
				RGBValue.append(priorityInfo.ledColors.begin()->green);
				RGBValue.append(priorityInfo.ledColors.begin()->blue);
				LEDcolor.insert("RGB Value", RGBValue);

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
				LEDcolor.insert("HSL Value", HSLValue);

				activeLedColors.append(LEDcolor);
			}
		}
		info["activeLedColor"] = activeLedColors;

		// END

		sendSuccessDataReply(QJsonDocument(info), command, tan);

		// AFTER we send the info, the client might want to subscribe to future updates
		if (message.contains("subscribe"))
		{
			// check if listeners are allowed
			if (_noListener)
				return;

			QJsonArray subsArr = message["subscribe"].toArray();
			// catch the all keyword and build a list of all cmds
			if (subsArr.contains("all"))
			{
				subsArr = QJsonArray();
				for (const auto& entry : _jsonCB->getCommands())
				{
					subsArr.append(entry);
				}
			}

			for (const QJsonValueRef entry : subsArr)
			{
				// config callbacks just if auth is set
				if ((entry == "settings-update" || entry == "token-update") && !API::isAdminAuthorized())
					continue;
				// silent failure if a subscribe type is not found
				_jsonCB->subscribeFor(entry.toString());
			}
		}
	}
	catch (...)
	{
		sendErrorReply("Exception");
	}
}

void JsonAPI::handleClearCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit forwardJsonMessage(message);
	int priority = message["priority"].toInt();
	QString replyMsg;

	if (!API::clearPriority(priority, replyMsg))
	{
		sendErrorReply(replyMsg, command, tan);
		return;
	}
	sendSuccessReply(command, tan);
}

void JsonAPI::handleClearallCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit forwardJsonMessage(message);
	QString replyMsg;
	API::clearPriority(-1, replyMsg);
	sendSuccessReply(command, tan);
}

void JsonAPI::handleHelpCommand(const QJsonObject& message, const QString& command, int tan)
{
	QJsonObject req;

	req["available_commands"] = "color, image, effect, serverinfo, clear, clearall, adjustment, sourceselect, config, componentstate, ledcolors, logging, processing, sysinfo, videomodehdr, videomode, video-crop, authorize, instance, leddevice, transform, correction, temperature, help";
	sendSuccessDataReply(QJsonDocument(req), command, tan);
}

void JsonAPI::handleCropCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QJsonObject& adjustment = message["crop"].toObject();
	int l = adjustment["left"].toInt(0);
	int r = adjustment["right"].toInt(0);
	int t = adjustment["top"].toInt(0);
	int b = adjustment["bottom"].toInt(0);

	if (GrabberWrapper::getInstance() != nullptr)
		emit GrabberWrapper::getInstance()->setCropping(l, r, t, b);

	sendSuccessReply(command, tan);
}

void JsonAPI::handleBenchmarkCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString& subc = message["subcommand"].toString().trimmed();
	int status = message["status"].toInt();

	if (GrabberWrapper::getInstance() != nullptr)
	{
		if (subc == "ping")
		{
			emit GrabberWrapper::getInstance()->benchmarkUpdate(status, "pong");
		}
		else
		{
			GrabberWrapper::getInstance()->benchmarkCapture(status, subc);
		}
	}

	sendSuccessReply(command, tan);
}

void JsonAPI::handleVideoControlsCommand(const QJsonObject& message, const QString& command, int tan)
{

#if defined(__APPLE__)
	sendErrorReply("Setting video controls is not supported under macOS", command, tan);
	return;
#endif

	const QJsonObject& adjustment = message["video-controls"].toObject();
	int hardware_brightness = adjustment["hardware_brightness"].toInt();
	int hardware_contrast = adjustment["hardware_contrast"].toInt();
	int hardware_saturation = adjustment["hardware_saturation"].toInt();
	int hardware_hue = adjustment["hardware_hue"].toInt();

	if (GrabberWrapper::getInstance() != nullptr)
		emit GrabberWrapper::getInstance()->setBrightnessContrastSaturationHue(hardware_brightness, hardware_contrast, hardware_saturation, hardware_hue);

	sendSuccessReply(command, tan);
}

void JsonAPI::handleAdjustmentCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QJsonObject& adjustment = message["adjustment"].toObject();

	const QString adjustmentId = adjustment["id"].toString(_hyperhdr->getAdjustmentIds().first());
	ColorAdjustment* colorAdjustment = _hyperhdr->getAdjustment(adjustmentId);
	if (colorAdjustment == nullptr)
	{
		Warning(_log, "Incorrect adjustment identifier: %s", adjustmentId.toStdString().c_str());
		return;
	}

	if (adjustment.contains("classic_config"))
	{
		colorAdjustment->_rgbTransform.setClassicConfig(adjustment["classic_config"].toBool(false));
	}

	if (adjustment.contains("temperatureRed"))
	{
		colorAdjustment->_rgbRedAdjustment.setCorrection(adjustment["temperatureRed"].toInt());
	}

	if (adjustment.contains("temperatureGreen"))
	{
		colorAdjustment->_rgbGreenAdjustment.setCorrection(adjustment["temperatureGreen"].toInt());
	}

	if (adjustment.contains("temperatureBlue"))
	{
		colorAdjustment->_rgbBlueAdjustment.setCorrection(adjustment["temperatureBlue"].toInt());
	}

	if (adjustment.contains("red"))
	{
		const QJsonArray& values = adjustment["red"].toArray();
		colorAdjustment->_rgbRedAdjustment.setAdjustment(values[0u].toInt(), values[1u].toInt(), values[2u].toInt());
	}

	if (adjustment.contains("green"))
	{
		const QJsonArray& values = adjustment["green"].toArray();
		colorAdjustment->_rgbGreenAdjustment.setAdjustment(values[0u].toInt(), values[1u].toInt(), values[2u].toInt());
	}

	if (adjustment.contains("blue"))
	{
		const QJsonArray& values = adjustment["blue"].toArray();
		colorAdjustment->_rgbBlueAdjustment.setAdjustment(values[0u].toInt(), values[1u].toInt(), values[2u].toInt());
	}
	if (adjustment.contains("cyan"))
	{
		const QJsonArray& values = adjustment["cyan"].toArray();
		colorAdjustment->_rgbCyanAdjustment.setAdjustment(values[0u].toInt(), values[1u].toInt(), values[2u].toInt());
	}
	if (adjustment.contains("magenta"))
	{
		const QJsonArray& values = adjustment["magenta"].toArray();
		colorAdjustment->_rgbMagentaAdjustment.setAdjustment(values[0u].toInt(), values[1u].toInt(), values[2u].toInt());
	}
	if (adjustment.contains("yellow"))
	{
		const QJsonArray& values = adjustment["yellow"].toArray();
		colorAdjustment->_rgbYellowAdjustment.setAdjustment(values[0u].toInt(), values[1u].toInt(), values[2u].toInt());
	}
	if (adjustment.contains("white"))
	{
		const QJsonArray& values = adjustment["white"].toArray();
		colorAdjustment->_rgbWhiteAdjustment.setAdjustment(values[0u].toInt(), values[1u].toInt(), values[2u].toInt());
	}

	if (adjustment.contains("gammaRed"))
	{
		colorAdjustment->_rgbTransform.setGamma(adjustment["gammaRed"].toDouble(), colorAdjustment->_rgbTransform.getGammaG(), colorAdjustment->_rgbTransform.getGammaB());
	}
	if (adjustment.contains("gammaGreen"))
	{
		colorAdjustment->_rgbTransform.setGamma(colorAdjustment->_rgbTransform.getGammaR(), adjustment["gammaGreen"].toDouble(), colorAdjustment->_rgbTransform.getGammaB());
	}
	if (adjustment.contains("gammaBlue"))
	{
		colorAdjustment->_rgbTransform.setGamma(colorAdjustment->_rgbTransform.getGammaR(), colorAdjustment->_rgbTransform.getGammaG(), adjustment["gammaBlue"].toDouble());
	}

	if (adjustment.contains("backlightThreshold"))
	{
		colorAdjustment->_rgbTransform.setBacklightThreshold(adjustment["backlightThreshold"].toDouble());
	}
	if (adjustment.contains("backlightColored"))
	{
		colorAdjustment->_rgbTransform.setBacklightColored(adjustment["backlightColored"].toBool());
	}
	if (adjustment.contains("brightness"))
	{
		colorAdjustment->_rgbTransform.setBrightness(adjustment["brightness"].toInt());
	}
	if (adjustment.contains("brightnessCompensation"))
	{
		colorAdjustment->_rgbTransform.setBrightnessCompensation(adjustment["brightnessCompensation"].toInt());
	}
	if (adjustment.contains("saturationGain"))
	{
		colorAdjustment->_rgbTransform.setSaturationGain(adjustment["saturationGain"].toDouble());
	}
	if (adjustment.contains("luminanceGain"))
	{
		colorAdjustment->_rgbTransform.setLuminanceGain(adjustment["luminanceGain"].toDouble());
	}

	// commit the changes
	QMetaObject::invokeMethod(_hyperhdr, "adjustmentsUpdated");
	sendSuccessReply(command, tan);
}

void JsonAPI::handleSourceSelectCommand(const QJsonObject& message, const QString& command, int tan)
{
	if (message.contains("auto"))
	{
		API::setSourceAutoSelect(message["auto"].toBool(false));
	}
	else if (message.contains("priority"))
	{
		API::setVisiblePriority(message["priority"].toInt());
	}
	else
	{
		sendErrorReply("Priority request is invalid", command, tan);
		return;
	}
	sendSuccessReply(command, tan);
}

void JsonAPI::handleSaveDB(const QJsonObject& message, const QString& command, int tan)
{
	if (_adminAuthorized)
	{
		QJsonObject backup = _instanceManager->getBackup();
		if (!backup.empty())
			sendSuccessDataReply(QJsonDocument(backup), command, tan);
		else
			sendErrorReply("Error while generating the backup file, please consult the HyperHDR logs.", command, tan);
	}
	else
		sendErrorReply("No Authorization", command, tan);
}

void JsonAPI::handleLoadDB(const QJsonObject& message, const QString& command, int tan)
{
	if (_adminAuthorized)
	{
		QString error = _instanceManager->restoreBackup(message);
		if (error.isEmpty())
		{
#ifdef __linux__
			raise(SIGSEGV);
#else
			QCoreApplication::quit();
#endif
		}
		else
			sendErrorReply("Error occured while restoring the backup: " + error, command, tan);
	}
	else
		sendErrorReply("No Authorization", command, tan);
}

void JsonAPI::handlePerformanceCounters(const QJsonObject& message, const QString& command, int tan)
{
	QString subcommand = message["subcommand"].toString("");
	QString full_command = command + "-" + subcommand;

	if (subcommand == "all")
	{
		emit PerformanceCounters::getInstance()->performanceInfoRequest(true);
		sendSuccessReply(command, tan);
	}
	else if (subcommand == "resources")
	{
		emit PerformanceCounters::getInstance()->performanceInfoRequest(false);
		sendSuccessReply(command, tan);
	}
	else
		sendErrorReply("Unknown subcommand", command, tan);
}

void JsonAPI::handleLoadSignalCalibration(const QJsonObject& message, const QString& command, int tan)
{
	QString subcommand = message["subcommand"].toString("");
	QString full_command = command + "-" + subcommand;

	if (GrabberWrapper::getInstance() == nullptr)
	{
		sendErrorReply("No grabbers available", command, tan);
		return;
	}

	if (subcommand == "start")
	{
		if (_adminAuthorized)
			sendSuccessDataReply(GrabberWrapper::getInstance()->startCalibration(), full_command, tan);
		else
			sendErrorReply("No Authorization", command, tan);
	}
	else if (subcommand == "stop")
	{
		sendSuccessDataReply(GrabberWrapper::getInstance()->stopCalibration(), full_command, tan);
	}
	else if (subcommand == "get-info")
	{
		sendSuccessDataReply(GrabberWrapper::getInstance()->getCalibrationInfo(), full_command, tan);
	}
	else
		sendErrorReply("Unknown subcommand", command, tan);
}

void JsonAPI::handleConfigCommand(const QJsonObject& message, const QString& command, int tan)
{
	QString subcommand = message["subcommand"].toString("");
	QString full_command = command + "-" + subcommand;

	if (subcommand == "getschema")
	{
		handleSchemaGetCommand(message, full_command, tan);
	}
	else if (subcommand == "setconfig")
	{
		if (_adminAuthorized)
			handleConfigSetCommand(message, full_command, tan);
		else
			sendErrorReply("No Authorization", command, tan);
	}
	else if (subcommand == "getconfig")
	{
		if (_adminAuthorized)
			sendSuccessDataReply(QJsonDocument(_hyperhdr->getQJsonConfig()), full_command, tan);
		else
			sendErrorReply("No Authorization", command, tan);
	}
	else
	{
		sendErrorReply("unknown or missing subcommand", full_command, tan);
	}
}

void JsonAPI::handleConfigSetCommand(const QJsonObject& message, const QString& command, int tan)
{
	if (message.contains("config"))
	{
		QJsonObject config = message["config"].toObject();
		if (API::isHyperhdrEnabled())
		{
			if (API::saveSettings(config))
			{
				sendSuccessReply(command, tan);
			}
			else
			{
				sendErrorReply("Save settings failed", command, tan);
			}
		}
		else
			sendErrorReply("Saving configuration while HyperHDR is disabled isn't possible", command, tan);
	}
}

void JsonAPI::handleSchemaGetCommand(const QJsonObject& message, const QString& command, int tan)
{
	// create result
	QJsonObject schemaJson, alldevices, properties;

	// make sure the resources are loaded (they may be left out after static linking)
	Q_INIT_RESOURCE(resource);

	// read the hyperhdr json schema from the resource
	QString schemaFile = ":/hyperhdr-schema";

	try
	{
		schemaJson = QJsonUtils::readSchema(schemaFile);
	}
	catch (const std::runtime_error& error)
	{
		throw std::runtime_error(error.what());
	}

	// collect all LED Devices
	properties = schemaJson["properties"].toObject();
	alldevices = LedDeviceWrapper::getLedDeviceSchemas();
	properties.insert("alldevices", alldevices);

	// collect all available effect schemas
	QJsonObject pyEffectSchemas, pyEffectSchema;
	QJsonArray in, ex;


	if (!in.empty())
		pyEffectSchema.insert("internal", in);
	if (!ex.empty())
		pyEffectSchema.insert("external", ex);

	pyEffectSchemas = pyEffectSchema;
	properties.insert("effectSchemas", pyEffectSchemas);

	schemaJson.insert("properties", properties);

	// send the result
	sendSuccessDataReply(QJsonDocument(schemaJson), command, tan);
}

void JsonAPI::handleComponentStateCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QJsonObject& componentState = message["componentstate"].toObject();
	QString comp = componentState["component"].toString("invalid");
	bool compState = componentState["state"].toBool(true);
	QString replyMsg;

	if (!API::setComponentState(comp, compState, replyMsg))
	{
		sendErrorReply(replyMsg, command, tan);
		return;
	}
	sendSuccessReply(command, tan);
}

void JsonAPI::handleLedColorsCommand(const QJsonObject& message, const QString& command, int tan)
{
	// create result
	QString subcommand = message["subcommand"].toString("");

	// max 20 Hz (50ms) interval for streaming (default: 10 Hz (100ms))
	qint64 streaming_interval = qMax(message["interval"].toInt(100), 50);

	if (subcommand == "ledstream-start")
	{
		_streaming_leds_reply["success"] = true;
		_streaming_leds_reply["command"] = command + "-ledstream-update";
		_streaming_leds_reply["tan"] = tan;

		connect(_hyperhdr, &HyperHdrInstance::rawLedColors, this, [=](const std::vector<ColorRgb>& ledValues) {
			_currentLedValues = ledValues;

			// necessary because Qt::UniqueConnection for lambdas does not work until 5.9
			// see: https://bugreports.qt.io/browse/QTBUG-52438
			if (!_ledStreamConnection)
				_ledStreamConnection = connect(_ledStreamTimer, &QTimer::timeout, this, [=]() {
				emit streamLedcolorsUpdate(_currentLedValues);
					},
					Qt::UniqueConnection);

			// start the timer
			if (!_ledStreamTimer->isActive() || _ledStreamTimer->interval() != streaming_interval)
				_ledStreamTimer->start(streaming_interval);
			},
			Qt::UniqueConnection);
		// push once
		QMetaObject::invokeMethod(_hyperhdr, "update");
	}
	else if (subcommand == "ledstream-stop")
	{
		disconnect(_hyperhdr, &HyperHdrInstance::rawLedColors, this, 0);
		_ledStreamTimer->stop();
		disconnect(_ledStreamConnection);
	}
	else if (subcommand == "imagestream-start")
	{
		_streaming_image_reply["success"] = true;
		_streaming_image_reply["command"] = command + "-imagestream-update";
		_streaming_image_reply["tan"] = tan;

		connect(_hyperhdr, &HyperHdrInstance::currentImage, this, &JsonAPI::setImage, Qt::UniqueConnection);
	}
	else if (subcommand == "imagestream-stop")
	{
		disconnect(_hyperhdr, &HyperHdrInstance::currentImage, this, 0);
	}
	else
	{
		return;
	}

	sendSuccessReply(command + "-" + subcommand, tan);
}

void JsonAPI::handleLoggingCommand(const QJsonObject& message, const QString& command, int tan)
{
	// create result
	QString subcommand = message["subcommand"].toString("");

	if (API::isAdminAuthorized())
	{
		_streaming_logging_reply["success"] = true;
		_streaming_logging_reply["command"] = command;
		_streaming_logging_reply["tan"] = tan;

		if (subcommand == "start")
		{
			if (!_streaming_logging_activated)
			{
				_streaming_logging_reply["command"] = command + "-update";
				connect(LoggerManager::getInstance(), &LoggerManager::newLogMessage, this, &JsonAPI::incommingLogMessage);
				Debug(_log, "log streaming activated for client %s", _peerAddress.toStdString().c_str()); // needed to trigger log sending
			}
		}
		else if (subcommand == "stop")
		{
			if (_streaming_logging_activated)
			{
				disconnect(LoggerManager::getInstance(), &LoggerManager::newLogMessage, this, &JsonAPI::incommingLogMessage);
				_streaming_logging_activated = false;
				Debug(_log, "log streaming deactivated for client  %s", _peerAddress.toStdString().c_str());
			}
		}
		else
		{
			return;
		}

		sendSuccessReply(command + "-" + subcommand, tan);
	}
	else
	{
		sendErrorReply("No Authorization", command + "-" + subcommand, tan);
	}
}

void JsonAPI::handleProcessingCommand(const QJsonObject& message, const QString& command, int tan)
{
	API::setLedMappingType(ImageProcessor::mappingTypeToInt(message["mappingType"].toString("multicolor_mean")));
	sendSuccessReply(command, tan);
}

void JsonAPI::handleVideoModeHdrCommand(const QJsonObject& message, const QString& command, int tan)
{
	API::setVideoModeHdr(message["HDR"].toInt());
	sendSuccessReply(command, tan);
}

void JsonAPI::handleLutCalibrationCommand(const QJsonObject& message, const QString& command, int tan)
{
	QString subcommand = message["subcommand"].toString("");
	int checksum = message["checksum"].toInt(-1);
	QJsonObject startColor = message["startColor"].toObject();
	QJsonObject endColor = message["endColor"].toObject();
	bool limitedRange = message["limitedRange"].toBool(false);
	double saturation = message["saturation"].toDouble(1.0);
	double luminance = message["luminance"].toDouble(1.0);
	double gammaR = message["gammaR"].toDouble(1.0);
	double gammaG = message["gammaG"].toDouble(1.0);
	double gammaB = message["gammaB"].toDouble(1.0);
	int coef = message["coef"].toInt(0);
	ColorRgb _startColor,_endColor;

	_startColor.red = startColor["r"].toInt(128);
	_startColor.green = startColor["g"].toInt(128);
	_startColor.blue = startColor["b"].toInt(128);
	_endColor.red = endColor["r"].toInt(255);
	_endColor.green = endColor["g"].toInt(255);
	_endColor.blue = endColor["b"].toInt(255);

	if (subcommand == "capture")	
		emit LutCalibrator::getInstance()->assign(checksum, _startColor, _endColor, limitedRange, saturation, luminance, gammaR, gammaG, gammaB, coef);
	else
		emit LutCalibrator::getInstance()->stop();

	sendSuccessReply(command, tan);
}

void JsonAPI::handleAuthorizeCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString& subc = message["subcommand"].toString().trimmed();
	const QString& id = message["id"].toString().trimmed();
	const QString& password = message["password"].toString().trimmed();
	const QString& newPassword = message["newPassword"].toString().trimmed();
	const QString& comment = message["comment"].toString().trimmed();

	// catch test if auth is required
	if (subc == "tokenRequired")
	{
		QJsonObject req;
		req["required"] = !API::isAuthorized();

		sendSuccessDataReply(QJsonDocument(req), command + "-" + subc, tan);
		return;
	}

	// catch test if admin auth is required
	if (subc == "adminRequired")
	{
		QJsonObject req;
		req["adminRequired"] = !API::isAdminAuthorized();
		sendSuccessDataReply(QJsonDocument(req), command + "-" + subc, tan);
		return;
	}

	// default hyperhdr password is a security risk, replace it asap
	if (subc == "newPasswordRequired")
	{
		QJsonObject req;
		req["newPasswordRequired"] = API::hasHyperhdrDefaultPw();
		sendSuccessDataReply(QJsonDocument(req), command + "-" + subc, tan);
		return;
	}

	// catch logout
	if (subc == "logout")
	{
		// disconnect all kind of data callbacks
		JsonAPI::stopDataConnections(); // TODO move to API
		API::logout();
		sendSuccessReply(command + "-" + subc, tan);
		return;
	}

	// change password
	if (subc == "newPassword")
	{
		// use password, newPassword
		if (API::isAdminAuthorized())
		{
			if (API::updateHyperhdrPassword(password, newPassword))
			{
				sendSuccessReply(command + "-" + subc, tan);
				return;
			}
			sendErrorReply("Failed to update user password", command + "-" + subc, tan);
			return;
		}
		sendErrorReply("No Authorization", command + "-" + subc, tan);
		return;
	}

	// token created from ui
	if (subc == "createToken")
	{
		// use comment
		// for user authorized sessions
		AuthManager::AuthDefinition def;
		const QString res = API::createToken(comment, def);
		if (res.isEmpty())
		{
			QJsonObject newTok;
			newTok["comment"] = def.comment;
			newTok["id"] = def.id;
			newTok["token"] = def.token;

			sendSuccessDataReply(QJsonDocument(newTok), command + "-" + subc, tan);
			return;
		}
		sendErrorReply(res, command + "-" + subc, tan);
		return;
	}

	// rename Token
	if (subc == "renameToken")
	{
		// use id/comment
		const QString res = API::renameToken(id, comment);
		if (res.isEmpty())
		{
			sendSuccessReply(command + "-" + subc, tan);
			return;
		}
		sendErrorReply(res, command + "-" + subc, tan);
		return;
	}

	// delete token
	if (subc == "deleteToken")
	{
		// use id
		const QString res = API::deleteToken(id);
		if (res.isEmpty())
		{
			sendSuccessReply(command + "-" + subc, tan);
			return;
		}
		sendErrorReply(res, command + "-" + subc, tan);
		return;
	}

	// catch token request
	if (subc == "requestToken")
	{
		// use id/comment
		const QString& comment = message["comment"].toString().trimmed();
		const bool& acc = message["accept"].toBool(true);
		if (acc)
			API::setNewTokenRequest(comment, id, tan);
		else
			API::cancelNewTokenRequest(comment, id);
		// client should wait for answer
		return;
	}

	// get pending token requests
	if (subc == "getPendingTokenRequests")
	{
		QVector<AuthManager::AuthDefinition> vec;
		if (API::getPendingTokenRequests(vec))
		{
			QJsonArray arr;
			for (const auto& entry : vec)
			{
				QJsonObject obj;
				obj["comment"] = entry.comment;
				obj["id"] = entry.id;
				obj["timeout"] = int(entry.timeoutTime);
				arr.append(obj);
			}
			sendSuccessDataReply(QJsonDocument(arr), command + "-" + subc, tan);
		}
		else
		{
			sendErrorReply("No Authorization", command + "-" + subc, tan);
		}

		return;
	}

	// accept/deny token request
	if (subc == "answerRequest")
	{
		// use id
		const bool& accept = message["accept"].toBool(false);
		if (!API::handlePendingTokenRequest(id, accept))
			sendErrorReply("No Authorization", command + "-" + subc, tan);
		return;
	}

	// get token list
	if (subc == "getTokenList")
	{
		QVector<AuthManager::AuthDefinition> defVect;
		if (API::getTokenList(defVect))
		{
			QJsonArray tArr;
			for (const auto& entry : defVect)
			{
				QJsonObject subO;
				subO["comment"] = entry.comment;
				subO["id"] = entry.id;
				subO["last_use"] = entry.lastUse;

				tArr.append(subO);
			}
			sendSuccessDataReply(QJsonDocument(tArr), command + "-" + subc, tan);
			return;
		}
		sendErrorReply("No Authorization", command + "-" + subc, tan);
		return;
	}

	// login
	if (subc == "login")
	{
		const QString& token = message["token"].toString().trimmed();

		// catch token
		if (!token.isEmpty())
		{
			// userToken is longer
			if (token.count() > 36)
			{
				if (API::isUserTokenAuthorized(token))
					sendSuccessReply(command + "-" + subc, tan);
				else
					sendErrorReply("No Authorization", command + "-" + subc, tan);

				return;
			}
			// usual app token is 36
			if (token.count() == 36)
			{
				if (API::isTokenAuthorized(token))
				{
					sendSuccessReply(command + "-" + subc, tan);
				}
				else
					sendErrorReply("No Authorization", command + "-" + subc, tan);
			}
			return;
		}

		// password
		// use password
		if (password.count() >= 8)
		{
			QString userTokenRep;
			if (API::isUserAuthorized(password) && API::getUserToken(userTokenRep))
			{
				// Return the current valid HyperHDR user token
				QJsonObject obj;
				obj["token"] = userTokenRep;
				sendSuccessDataReply(QJsonDocument(obj), command + "-" + subc, tan);
			}
			else
				sendErrorReply("No Authorization", command + "-" + subc, tan);
		}
		else
			sendErrorReply("Password too short", command + "-" + subc, tan);
	}
}

void JsonAPI::handleInstanceCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString& subc = message["subcommand"].toString();
	const quint8& inst = message["instance"].toInt();
	const QString& name = message["name"].toString();

	if (subc == "switchTo")
	{
		if (handleInstanceSwitch(inst))
		{
			QJsonObject obj;
			obj["instance"] = inst;
			sendSuccessDataReply(QJsonDocument(obj), command + "-" + subc, tan);
		}
		else
			sendErrorReply("Selected HyperHDR instance isn't running", command + "-" + subc, tan);
		return;
	}

	if (subc == "startInstance")
	{
		connect(this, &API::onStartInstanceResponse, [=](const int& tan) { sendSuccessReply(command + "-" + subc, tan); });
		if (!API::startInstance(inst, tan))
			sendErrorReply("Can't start HyperHDR instance index " + QString::number(inst), command + "-" + subc, tan);
		return;
	}

	if (subc == "stopInstance")
	{
		// silent fail
		API::stopInstance(inst);
		sendSuccessReply(command + "-" + subc, tan);
		return;
	}

	if (subc == "deleteInstance")
	{
		QString replyMsg;
		if (API::deleteInstance(inst, replyMsg))
			sendSuccessReply(command + "-" + subc, tan);
		else
			sendErrorReply(replyMsg, command + "-" + subc, tan);
		return;
	}

	// create and save name requires name
	if (name.isEmpty())
		sendErrorReply("Name string required for this command", command + "-" + subc, tan);

	if (subc == "createInstance")
	{
		QString replyMsg = API::createInstance(name);
		if (replyMsg.isEmpty())
			sendSuccessReply(command + "-" + subc, tan);
		else
			sendErrorReply(replyMsg, command + "-" + subc, tan);
		return;
	}

	if (subc == "saveName")
	{
		QString replyMsg = API::setInstanceName(inst, name);
		if (replyMsg.isEmpty())
			sendSuccessReply(command + "-" + subc, tan);
		else
			sendErrorReply(replyMsg, command + "-" + subc, tan);
		return;
	}
}

void JsonAPI::handleLedDeviceCommand(const QJsonObject& message, const QString& command, int tan)
{
	Debug(_log, "message: [%s]", QString(QJsonDocument(message).toJson(QJsonDocument::Compact)).toUtf8().constData());

	const QString& subc = message["subcommand"].toString().trimmed();
	const QString& devType = message["ledDeviceType"].toString().trimmed();

	QString full_command = command + "-" + subc;

	// TODO: Validate that device type is a valid one
/*	if ( ! valid type )
	{
		sendErrorReply("Unknown device", full_command, tan);
	}
	else
*/ {
		QJsonObject config;
		config.insert("type", devType);
		LedDevice* ledDevice = nullptr;

		if (subc == "discover")
		{
			ledDevice = LedDeviceFactory::construct(config);
			const QJsonObject& params = message["params"].toObject();
			const QJsonObject devicesDiscovered = ledDevice->discover(params);

			Debug(_log, "response: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

			sendSuccessDataReply(QJsonDocument(devicesDiscovered), full_command, tan);
		}
		else if (subc == "getProperties")
		{
			ledDevice = LedDeviceFactory::construct(config);
			const QJsonObject& params = message["params"].toObject();
			const QJsonObject deviceProperties = ledDevice->getProperties(params);

			Debug(_log, "response: [%s]", QString(QJsonDocument(deviceProperties).toJson(QJsonDocument::Compact)).toUtf8().constData());

			sendSuccessDataReply(QJsonDocument(deviceProperties), full_command, tan);
		}
		else if (subc == "identify")
		{
			ledDevice = LedDeviceFactory::construct(config);
			const QJsonObject& params = message["params"].toObject();
			ledDevice->identify(params);

			sendSuccessReply(full_command, tan);
		}
		else
		{
			sendErrorReply("Unknown or missing subcommand", full_command, tan);
		}

		delete  ledDevice;
	}
}

void JsonAPI::handleNotImplemented(const QString& command, int tan)
{
	sendErrorReply("Command not implemented", command, tan);
}

void JsonAPI::sendSuccessReply(const QString& command, int tan)
{
	// create reply
	QJsonObject reply;
	reply["success"] = true;
	reply["command"] = command;
	reply["tan"] = tan;

	// send reply
	emit callbackMessage(reply);
}

void JsonAPI::sendSuccessDataReply(const QJsonDocument& doc, const QString& command, int tan)
{
	QJsonObject reply;
	reply["success"] = true;
	reply["command"] = command;
	reply["tan"] = tan;
	if (doc.isArray())
		reply["info"] = doc.array();
	else
		reply["info"] = doc.object();

	emit callbackMessage(reply);
}

void JsonAPI::sendErrorReply(const QString& error, const QString& command, int tan)
{
	// create reply
	QJsonObject reply;
	reply["success"] = false;
	reply["error"] = error;
	reply["command"] = command;
	reply["tan"] = tan;

	// send reply
	emit callbackMessage(reply);
}

void JsonAPI::streamLedcolorsUpdate(const std::vector<ColorRgb>& ledColors)
{
	QJsonObject result;
	QJsonArray leds;

	for (const auto& color : ledColors)
	{
		leds << QJsonValue(color.red) << QJsonValue(color.green) << QJsonValue(color.blue);
	}

	result["leds"] = leds;
	_streaming_leds_reply["result"] = result;

	// send the result
	emit callbackMessage(_streaming_leds_reply);
}

void JsonAPI::setImage(const Image<ColorRgb>& image)
{	
	uint64_t _currentTime = QDateTime::currentMSecsSinceEpoch();

	if (!_semaphore.tryAcquire() && (_lastSendImage < _currentTime && (_currentTime - _lastSendImage < 2000)))	
		return;	

	_lastSendImage = _currentTime;

	QImage jpgImage((const uint8_t*)image.memptr(), image.width(), image.height(), 3 * image.width(), QImage::Format_RGB888);
	QByteArray ba;
	QBuffer buffer(&ba);
	buffer.open(QIODevice::WriteOnly);

	if (image.width() > 1920)
	{		
		jpgImage = jpgImage.scaled(image.width() / 2, image.height() / 2);
	}

	jpgImage.save(&buffer, "jpg");

	QJsonObject result;
	result["image"] = "data:image/jpg;base64," + QString(ba.toBase64());
	_streaming_image_reply["result"] = result;
	_streaming_image_reply["isImage"] = 1;

	emit callbackMessage(_streaming_image_reply);
}

void JsonAPI::releaseLock()
{
	_semaphore.release();
}

void JsonAPI::incommingLogMessage(const Logger::T_LOG_MESSAGE& msg)
{
	QJsonObject result, message;
	QJsonArray messageArray;

	if (!_streaming_logging_activated)
	{
		_streaming_logging_activated = true;
		const QList<Logger::T_LOG_MESSAGE>* logBuffer = LoggerManager::getInstance()->getLogMessageBuffer();
		for (int i = 0; i < logBuffer->length(); i++)
		{
			message["appName"] = logBuffer->at(i).appName;
			message["loggerName"] = logBuffer->at(i).loggerName;
			message["function"] = logBuffer->at(i).function;
			message["line"] = QString::number(logBuffer->at(i).line);
			message["fileName"] = logBuffer->at(i).fileName;
			message["message"] = logBuffer->at(i).message;
			message["levelString"] = logBuffer->at(i).levelString;
			message["utime"] = QString::number(logBuffer->at(i).utime);

			messageArray.append(message);
		}
	}
	else
	{
		message["appName"] = msg.appName;
		message["loggerName"] = msg.loggerName;
		message["function"] = msg.function;
		message["line"] = QString::number(msg.line);
		message["fileName"] = msg.fileName;
		message["message"] = msg.message;
		message["levelString"] = msg.levelString;
		message["utime"] = QString::number(msg.utime);

		messageArray.append(message);
	}

	result.insert("messages", messageArray);
	_streaming_logging_reply["result"] = result;

	// send the result
	emit callbackMessage(_streaming_logging_reply);
}

void JsonAPI::newPendingTokenRequest(const QString& id, const QString& comment)
{
	QJsonObject obj;
	obj["comment"] = comment;
	obj["id"] = id;
	obj["timeout"] = 180000;

	sendSuccessDataReply(QJsonDocument(obj), "authorize-tokenRequest", 1);
}

void JsonAPI::handleTokenResponse(bool success, const QString& token, const QString& comment, const QString& id, const int& tan)
{
	const QString cmd = "authorize-requestToken";
	QJsonObject result;
	result["token"] = token;
	result["comment"] = comment;
	result["id"] = id;

	if (success)
		sendSuccessDataReply(QJsonDocument(result), cmd, tan);
	else
		sendErrorReply("Token request timeout or denied", cmd, tan);
}

void JsonAPI::handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name)
{
	switch (state)
	{
	case InstanceState::H_ON_STOP:
		if (_hyperhdr->getInstanceIndex() == instance)
		{
			handleInstanceSwitch();
		}
		break;
	default:
		break;
	}
}

void JsonAPI::stopDataConnections()
{
	LoggerManager::getInstance()->disconnect();
	_streaming_logging_activated = false;
	_jsonCB->resetSubscriptions();
	// led stream colors
	disconnect(_hyperhdr, &HyperHdrInstance::rawLedColors, this, 0);
	_ledStreamTimer->stop();
	disconnect(_ledStreamConnection);
}
