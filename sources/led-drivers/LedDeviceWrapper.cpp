#include <led-drivers/LedDeviceWrapper.h>

#include <led-drivers/LedDevice.h>
#include <led-drivers/LedDeviceManufactory.h>

// util
#include <base/HyperHdrInstance.h>
#include <json-utils/JsonUtils.h>
#include <utils/Macros.h>

// qt
#include <future>
#include <QMutexLocker>
#include <QThread>
#include <QDir>

LedDeviceWrapper::LedDeviceWrapper(HyperHdrInstance* ownerInstance)
	: QObject(ownerInstance)
	, _ownerInstance(ownerInstance)
	, _ledDevice(nullptr, nullptr)
	, _enabled(false)
{
	_ownerInstance->setNewComponentState(hyperhdr::COMP_LEDDEVICE, false);
}

LedDeviceWrapper::~LedDeviceWrapper()
{
	_ledDevice.reset();
}

void LedDeviceWrapper::createLedDevice(QJsonObject config, int smoothingInterval, bool disableOnStartup)
{
	auto threadReadyPromisePtr = std::make_shared<std::promise<void>>();
	const int instanceIndex = _ownerInstance->getInstanceIndex();

	_ledDevice.reset();

	config["smoothingRefreshTime"] = smoothingInterval;

	QThread* thread = new QThread();
	thread->setObjectName(QString("LedDeviceThread%1").arg(instanceIndex));

	// start the thread
	thread->start();

	QObject::connect(thread, &QThread::started, this, [this, threadReadyPromisePtr, config, instanceIndex, disableOnStartup]() {
		_ledDevice = std::unique_ptr<LedDevice, void(*)(LedDevice*)>(
			hyperhdr::leds::CONSTRUCT_LED_DEVICE(config),
			[](LedDevice* oldLed) {
				QUEUE_CALL_0(oldLed, stop);
				hyperhdr::THREAD_REMOVER(QString("%1 [LedDevice]").arg(oldLed->thread()->objectName()), oldLed->thread(), oldLed);
			}
		);
		_ledDevice->setInstanceIndex(instanceIndex);

		// setup thread management
		if (!disableOnStartup)
			QUEUE_CALL_0(_ledDevice.get(), start);

		connect(_ownerInstance, &HyperHdrInstance::SignalSmoothingRestarted, _ledDevice.get(), &LedDevice::smoothingRestarted, Qt::QueuedConnection);
		connect(_ledDevice.get(), &LedDevice::SignalEnableStateChanged, this, &LedDeviceWrapper::handleInternalEnableState, Qt::QueuedConnection);

		threadReadyPromisePtr->set_value();
	}, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::DirectConnection));

	threadReadyPromisePtr->get_future().get();
}

void LedDeviceWrapper::handleComponentState(hyperhdr::Components component, bool state)
{
	if (_ledDevice == nullptr)
		return;

	if (component == hyperhdr::COMP_LEDDEVICE)
	{
		if (state)
		{
			QUEUE_CALL_0(_ledDevice.get(), enable);
		}
		else
		{
			QUEUE_CALL_0(_ledDevice.get(), disable);
		}
	}

	if (component == hyperhdr::COMP_ALL)
	{
		QUEUE_CALL_1(_ledDevice.get(), pauseRetryTimer, bool, (!state));
	}
}

void LedDeviceWrapper::handleInternalEnableState(bool newState)
{
	if (_ledDevice == nullptr)
		return;

	if (newState)
	{
		connect(_ledDevice.get(), &LedDevice::SignalSmoothingClockTick, _ownerInstance, &HyperHdrInstance::SignalSmoothingClockTick, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
		connect(_ownerInstance, &HyperHdrInstance::SignalFinalOutputColorsReady, _ledDevice.get(), &LedDevice::handleSignalFinalOutputColorsReady, Qt::UniqueConnection);
	}
	else
	{
		disconnect(_ledDevice.get(), &LedDevice::SignalSmoothingClockTick, _ownerInstance, &HyperHdrInstance::SignalSmoothingClockTick);
		disconnect(_ownerInstance, &HyperHdrInstance::SignalFinalOutputColorsReady, _ledDevice.get(), &LedDevice::handleSignalFinalOutputColorsReady);
	}

	_ownerInstance->setNewComponentState(hyperhdr::COMP_LEDDEVICE, newState);
	_enabled = newState;

	if (_enabled)
	{
		_ownerInstance->update();
	}
}


unsigned int LedDeviceWrapper::getLedCount() const
{
	int value = 0;
	if (_ledDevice != nullptr)
		SAFE_CALL_0_RET(_ledDevice.get(), getLedCount, int, value);
	return value;
}

QString LedDeviceWrapper::getActiveDeviceType() const
{
	QString value = nullptr;
	if (_ledDevice != nullptr)
		SAFE_CALL_0_RET(_ledDevice.get(), getActiveDeviceType, QString, value);
	return value;
}

bool LedDeviceWrapper::enabled() const
{
	return _enabled;
}

void LedDeviceWrapper::identifyLed(const QJsonObject& params)
{
	if (_ledDevice != nullptr)
		QUEUE_CALL_1(_ledDevice.get(), blinking, QJsonObject, params);
}

int LedDeviceWrapper::hasLedClock()
{
	int hasLedClock = 0;

	if (_ledDevice != nullptr)
		SAFE_CALL_0_RET(_ledDevice.get(), hasLedClock, int, hasLedClock);

	return hasLedClock;
}

QJsonObject LedDeviceWrapper::getLedDeviceSchemas()
{
	// make sure the resources are loaded (they may be left out after static linking)
	Q_INIT_RESOURCE(LedDeviceSchemas);

	// read the JSON schema from the resource
	QDir dir(":/leddevices/");
	QJsonObject result, schemaJson;
	LoggerName logger = "LedDevice";

	for (QString& item : dir.entryList())
	{
		QString schemaPath(QString(":/leddevices/") + item);
		QString devName = item.remove("schema-");

		QString data;
		if (!FileUtils::readFile(schemaPath, data, logger))
		{
			throw std::runtime_error("ERROR: Schema not found: " + item.toStdString());
		}

		QJsonObject schema;
		if (!JsonUtils::parse(schemaPath, data, schema, logger))
		{
			throw std::runtime_error("ERROR: JSON schema wrong of file: " + item.toStdString());
		}

		schemaJson = schema;
		schemaJson["title"] = QString("edt_dev_spec_header_title");

		result[devName] = schemaJson;
	}

	return result;
}
