#include <base/HyperHdrIManager.h>

#include <base/HyperHdrInstance.h>
#include <db/InstanceTable.h>
#include <base/GrabberWrapper.h>

// qt
#include <QThread>

HyperHdrIManager* HyperHdrIManager::HIMinstance;

HyperHdrIManager::HyperHdrIManager(const QString& rootPath, QObject* parent, bool readonlyMode)
	: QObject(parent)
	, _log(Logger::getInstance("HYPERMANAGER"))
	, _instanceTable(new InstanceTable(rootPath, this, readonlyMode))
	, _rootPath(rootPath)
	, _readonlyMode(readonlyMode)
	, _fireStarter(0)
{
	HIMinstance = this;
	qRegisterMetaType<InstanceState>("InstanceState");
	connect(this, &HyperHdrIManager::instanceStateChanged, this, &HyperHdrIManager::handleInstanceStateChange);
}

void HyperHdrIManager::handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name)
{
	switch (state)
	{
		case InstanceState::H_STARTED:
			emit PerformanceCounters::getInstance()->newCounter(
				PerformanceReport(static_cast<int>(PerformanceReportType::INSTANCE), -1, name, -1, -1, -1, -1, instance));
			break;
		case InstanceState::H_STOPPED:
			emit PerformanceCounters::getInstance()->removeCounter(static_cast<int>(PerformanceReportType::INSTANCE), instance);
			emit PerformanceCounters::getInstance()->removeCounter(static_cast<int>(PerformanceReportType::LED), instance);
			break;
		default:
			break;
	}
}


HyperHdrInstance* HyperHdrIManager::getHyperHdrInstance(quint8 instance)
{
	if (_runningInstances.contains(instance))
		return _runningInstances.value(instance);

	Warning(_log, "The requested instance index '%d' with name '%s' isn't running, return main instance", instance, QSTRING_CSTR(_instanceTable->getNamebyIndex(instance)));
	return _runningInstances.value(0);
}

QVector<QVariantMap> HyperHdrIManager::getInstanceData() const
{
	QVector<QVariantMap> instances = _instanceTable->getAllInstances();
	for (auto& entry : instances)
	{
		// add running state
		entry["running"] = _runningInstances.contains(entry["instance"].toInt());
	}
	return instances;
}

bool HyperHdrIManager::areInstancesReady()
{
	if (_fireStarter > 0)
		_fireStarter--;

	return (_fireStarter < 1);
}

void HyperHdrIManager::startAll()
{
	auto instanceList = _instanceTable->getAllInstances(true);

	_fireStarter = instanceList.count();

	for (const auto& entry : instanceList)
	{
		startInstance(entry["instance"].toInt());
	}

	if (GrabberWrapper::getInstance() != nullptr)
		emit setNewComponentStateToAllInstances(hyperhdr::Components::COMP_HDR, (GrabberWrapper::getInstance()->getHdrToneMappingEnabled() != 0));
}

void HyperHdrIManager::stopAll()
{
	// copy the instances due to loop corruption, even with .erase() return next iter
	QMap<quint8, HyperHdrInstance*> instCopy = _runningInstances;
	for (const auto instance : instCopy)
	{
		instance->stop();
	}
}

void HyperHdrIManager::setSmoothing(int time)
{
	QMap<quint8, HyperHdrInstance*> instCopy = _runningInstances;

	for (const auto instance : instCopy)
		QTimer::singleShot(0, instance, [=]() { instance->setSmoothing(time); });	
}

bool HyperHdrIManager::isCEC()
{
	QMap<quint8, HyperHdrInstance*> instCopy = _runningInstances;
	for (const auto instance : instCopy)
	{
		if (instance->isCEC())
			return true;
	}

	return false;
}

void HyperHdrIManager::setSignalStateByCEC(bool enable)
{
	QMap<quint8, HyperHdrInstance*> instCopy = _runningInstances;
	for (const auto instance : instCopy)
	{
		instance->setSignalStateByCEC(enable);
	}
}

void HyperHdrIManager::toggleStateAllInstances(bool pause)
{
	// copy the instances due to loop corruption, even with .erase() return next iter
	QMap<quint8, HyperHdrInstance*> instCopy = _runningInstances;
	for (const auto instance : instCopy)
	{
		emit instance->compStateChangeRequest(hyperhdr::COMP_ALL, pause);
	}
}

void HyperHdrIManager::hibernate(bool wakeUp)
{	
	if (!wakeUp)
	{
		Warning(_log, "The system is going to sleep");
		toggleStateAllInstances(false);
	}
	else
	{
		Warning(_log, "The system is going to wake up");
		QTimer::singleShot(3000, [this]() { toggleStateAllInstances(true);  });
	}	
}

bool HyperHdrIManager::startInstance(quint8 inst, bool block, QObject* caller, int tan)
{
	if (_instanceTable->instanceExist(inst))
	{
		if (!_runningInstances.contains(inst) && !_startQueue.contains(inst))
		{
			QThread* hyperhdrThread = new QThread();
			hyperhdrThread->setObjectName("HyperHdrThread");
			HyperHdrInstance* hyperhdr = new HyperHdrInstance(inst, _readonlyMode, _instanceTable->getNamebyIndex(inst));
			hyperhdr->moveToThread(hyperhdrThread);
			// setup thread management
			connect(hyperhdrThread, &QThread::started, hyperhdr, &HyperHdrInstance::start);
			connect(hyperhdr, &HyperHdrInstance::started, this, &HyperHdrIManager::handleStarted);
			connect(hyperhdr, &HyperHdrInstance::finished, this, &HyperHdrIManager::handleFinished);
			connect(hyperhdr, &HyperHdrInstance::finished, hyperhdrThread, &QThread::quit, Qt::DirectConnection);

			// setup further connections
			// from HyperHDR
			connect(hyperhdr, &HyperHdrInstance::settingsChanged, this, &HyperHdrIManager::settingsChanged);

			connect(hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &HyperHdrIManager::compStateChangeRequest);

			connect(this, &HyperHdrIManager::setNewComponentStateToAllInstances, hyperhdr, &HyperHdrInstance::setNewComponentState);

			// add to queue and start
			_startQueue << inst;
			hyperhdrThread->start();

			// update db
			_instanceTable->setLastUse(inst);
			_instanceTable->setEnable(inst, true);

			if (block)
			{
				while (!hyperhdrThread->isRunning()) {};
			}

			if (!_pendingRequests.contains(inst) && caller != nullptr)
			{
				PendingRequests newDef{ caller, tan };
				_pendingRequests[inst] = newDef;
			}

			return true;
		}
		Debug(_log, "Can't start Hyperhdr instance index '%d' with name '%s' it's already running or queued for start", inst, QSTRING_CSTR(_instanceTable->getNamebyIndex(inst)));
		return false;
	}
	Debug(_log, "Can't start Hyperhdr instance index '%d' it doesn't exist in DB", inst);
	return false;
}

bool HyperHdrIManager::stopInstance(quint8 inst)
{
	// inst 0 can't be stopped
	if (!isInstAllowed(inst))
		return false;

	if (_instanceTable->instanceExist(inst))
	{
		if (_runningInstances.contains(inst))
		{
			// notify a ON_STOP rather sooner than later, queued signal listener should have some time to drop the pointer before it's deleted
			emit instanceStateChanged(InstanceState::H_ON_STOP, inst);
			HyperHdrInstance* hyperhdr = _runningInstances.value(inst);
			hyperhdr->stop();

			// update db
			_instanceTable->setEnable(inst, false);

			return true;
		}
		Debug(_log, "Can't stop HyperHDR instance index '%d' with name '%s' it's not running'", inst, QSTRING_CSTR(_instanceTable->getNamebyIndex(inst)));
		return false;
	}
	Debug(_log, "Can't stop HyperHDR instance index '%d' it doesn't exist in DB", inst);
	return false;
}

bool HyperHdrIManager::createInstance(const QString& name, bool start)
{
	quint8 inst;
	if (_instanceTable->createInstance(name, inst))
	{
		Info(_log, "New Hyperhdr instance created with name '%s'", QSTRING_CSTR(name));
		emit instanceStateChanged(InstanceState::H_CREATED, inst, name);
		emit change();

		if (start)
			startInstance(inst);
		return true;
	}
	return false;
}

bool HyperHdrIManager::deleteInstance(quint8 inst)
{
	// inst 0 can't be deleted
	if (!isInstAllowed(inst))
		return false;

	// stop it if required as blocking and wait
	stopInstance(inst);

	if (_instanceTable->deleteInstance(inst))
	{
		Info(_log, "Hyperhdr instance with index '%d' has been deleted", inst);
		emit instanceStateChanged(InstanceState::H_DELETED, inst);
		emit change();

		return true;
	}
	return false;
}

bool HyperHdrIManager::saveName(quint8 inst, const QString& name)
{
	if (_instanceTable->saveName(inst, name))
	{
		emit change();
		return true;
	}
	return false;
}

void HyperHdrIManager::handleFinished()
{
	HyperHdrInstance* hyperhdr = qobject_cast<HyperHdrInstance*>(sender());
	quint8 instance = hyperhdr->getInstanceIndex();

	Info(_log, "HyperHDR instance '%s' has been stopped", QSTRING_CSTR(_instanceTable->getNamebyIndex(instance)));

	_runningInstances.remove(instance);
	hyperhdr->thread()->deleteLater();
	hyperhdr->deleteLater();
	emit instanceStateChanged(InstanceState::H_STOPPED, instance);
	emit change();
}

void HyperHdrIManager::handleStarted()
{
	HyperHdrInstance* hyperhdr = qobject_cast<HyperHdrInstance*>(sender());
	quint8 instance = hyperhdr->getInstanceIndex();

	Info(_log, "HyperHDR instance '%s' has been started", QSTRING_CSTR(_instanceTable->getNamebyIndex(instance)));

	_startQueue.removeAll(instance);
	_runningInstances.insert(instance, hyperhdr);
	emit instanceStateChanged(InstanceState::H_STARTED, instance, _instanceTable->getNamebyIndex(instance));
	emit change();

	if (_pendingRequests.contains(instance))
	{
		PendingRequests def = _pendingRequests.take(instance);
		emit startInstanceResponse(def.caller, def.tan);
		_pendingRequests.remove(instance);
	}
}

const QJsonObject HyperHdrIManager::getBackup()
{
	QJsonObject backup;

	if (_instanceTable != nullptr)
	{
		if (QThread::currentThread() == _instanceTable->thread())
			backup = _instanceTable->getBackup();
		else
			QMetaObject::invokeMethod(_instanceTable, "getBackup", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(QJsonObject, backup));
	}
	return backup;
}


QString HyperHdrIManager::restoreBackup(const QJsonObject& message)
{
	QString error("Empty instance table manager");

	if (_instanceTable != nullptr)
	{
		if (QThread::currentThread() == _instanceTable->thread())
			error = _instanceTable->restoreBackup(message);
		else
			QMetaObject::invokeMethod(_instanceTable, "restoreBackup", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(QString, error), Q_ARG(QJsonObject, message));
	}
	return error;
}

void HyperHdrIManager::saveCalibration(QString saveData)
{
	HyperHdrInstance* instance = HyperHdrIManager::getHyperHdrInstance(0);
	if (instance != nullptr)
		QMetaObject::invokeMethod(instance, "saveCalibration", Q_ARG(QString, saveData));
	else
		Error(_log, "Hyperhdr instance is not running...can't save the calibration data");
}
