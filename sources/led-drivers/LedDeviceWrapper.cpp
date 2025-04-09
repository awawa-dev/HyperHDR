#include <led-drivers/LedDeviceWrapper.h>

#include <led-drivers/LedDevice.h>
#include <led-drivers/LedDeviceManufactory.h>

// util
#include <base/HyperHdrInstance.h>
#include <json-utils/JsonUtils.h>
#include <utils/Macros.h>

// qt
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
	_ledDevice.reset();

	config["smoothingRefreshTime"] = smoothingInterval;

	QThread* thread = new QThread();
	thread->setObjectName("LedDeviceThread");

	_ledDevice = std::unique_ptr<LedDevice, void(*)(LedDevice*)>(
		hyperhdr::leds::CONSTRUCT_LED_DEVICE(config),
		[](LedDevice* oldLed) {
			QUEUE_CALL_0(oldLed, stop);
			hyperhdr::THREAD_REMOVER(QString("LedDevice"), oldLed->thread(), oldLed);
		}
	);
	_ledDevice->setInstanceIndex(_ownerInstance->getInstanceIndex());
	_ledDevice->moveToThread(thread);

	// setup thread management
	if (!disableOnStartup)
		connect(thread, &QThread::started, _ledDevice.get(), &LedDevice::start);
	connect(thread, &QThread::finished, _ledDevice.get(), &LedDevice::stop);
	connect(_ownerInstance, &HyperHdrInstance::SignalSmoothingRestarted, _ledDevice.get(), &LedDevice::smoothingRestarted, Qt::QueuedConnection);
	connect(_ledDevice.get(), &LedDevice::SignalEnableStateChanged, this, &LedDeviceWrapper::handleInternalEnableState, Qt::QueuedConnection);

	// start the thread
	thread->start();
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
		connect(_ownerInstance, &HyperHdrInstance::SignalUpdateLeds, _ledDevice.get(), &LedDevice::updateLeds, Qt::UniqueConnection);
	}
	else
	{
		disconnect(_ledDevice.get(), &LedDevice::SignalSmoothingClockTick, _ownerInstance, &HyperHdrInstance::SignalSmoothingClockTick);
		disconnect(_ownerInstance, &HyperHdrInstance::SignalUpdateLeds, _ledDevice.get(), &LedDevice::updateLeds);
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
	QString value = 0;
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

	for (QString& item : dir.entryList())
	{
		QString schemaPath(QString(":/leddevices/") + item);
		QString devName = item.remove("schema-");

		QString data;
		if (!FileUtils::readFile(schemaPath, data, Logger::getInstance("LedDevice")))
		{
			throw std::runtime_error("ERROR: Schema not found: " + item.toStdString());
		}

		QJsonObject schema;
		if (!JsonUtils::parse(schemaPath, data, schema, Logger::getInstance("LedDevice")))
		{
			throw std::runtime_error("ERROR: JSON schema wrong of file: " + item.toStdString());
		}

		schemaJson = schema;
		schemaJson["title"] = QString("edt_dev_spec_header_title");

		result[devName] = schemaJson;
	}

	return result;
}
