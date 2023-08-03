#pragma once

// util
#include <utils/Logger.h>
#include <utils/settings.h>
#include <utils/Components.h>

// qt
#include <QMap>

class HyperHdrInstance;
class InstanceTable;

enum class InstanceState {
	H_STARTED,
	H_ON_STOP,
	H_STOPPED,
	H_CREATED,
	H_DELETED
};

///
/// @brief HyperHDRInstanceManager manages the instances of the the HyperHDR class
///
class HyperHdrIManager : public QObject
{
	Q_OBJECT

public:
	struct PendingRequests
	{
		QObject* caller;
		int     tan;
	};

	// global instance pointer
	static HyperHdrIManager* getInstance() { return HIMinstance; }
	static HyperHdrIManager* HIMinstance;
	QString getRootPath() { return _rootPath; }
	bool areInstancesReady();

public slots:
	void setSmoothing(int time);

	bool isCEC();

	void setSignalStateByCEC(bool enable);

	const QJsonObject getBackup();

	QString restoreBackup(const QJsonObject& message);

	///
	/// @brief Is given instance running?
	/// @param inst  The instance to check
	/// @return  True when running else false
	///
	bool IsInstanceRunning(quint8 inst) const { return _runningInstances.contains(inst); }

	///
	/// @brief Get a HyperHDR instance by index
	/// @param intance  the index
	/// @return HyperHDR instance, if index is not found returns instance 0
	///
	HyperHdrInstance* getHyperHdrInstance(quint8 instance = 0);

	///
	/// @brief Get instance data of all instaces in db + running state
	///
	QVector<QVariantMap> getInstanceData() const;

	///
	/// @brief Start a HyperHDR instance
	/// @param instance     Instance index
	/// @param block        If true return when thread has been started
	/// @return Return true on success, false if not found in db
	///
	bool startInstance(quint8 inst, bool block = false, QObject* caller = nullptr, int tan = 0);

	///
	/// @brief Stop a HyperHDR instance
	/// @param instance  Instance index
	/// @return Return true on success, false if not found in db
	///
	bool stopInstance(quint8 inst);

	QJsonObject getAverageColor(quint8 index);

	///
	/// @brief Toggle the state of all HyperHDR instances
	/// @param pause If true all instances toggle to pause, else to resume
	///
	void toggleStateAllInstances(bool pause = false);

	void hibernate(bool wakeUp);

	///
	/// @brief Create a new HyperHDR instance entry in db
	/// @param name  The friendly name of the instance
	/// @param start     If true it will be started after creation (async)
	/// @return Return true on success false if name is already in use or a db error occurred
	///
	bool createInstance(const QString& name, bool start = false);

	///
	/// @brief Delete HyperHDR instance entry in db. Cleanup also all associated table data for this instance
	/// @param inst  The instance index
	/// @return Return true on success, false if not found or not allowed
	///
	bool deleteInstance(quint8 inst);

	///
	/// @brief Assign a new name to the given instance
	/// @param inst  The instance index
	/// @param name  The instance name index
	/// @return Return true on success, false if not found
	///
	bool saveName(quint8 inst, const QString& name);

	void saveCalibration(QString saveData);

	void handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name);

signals:
	///
	/// @brief Emits whenever the state of a instance changes according to enum instanceState
	/// @param instaneState  A state from enum
	/// @param instance      The index of instance
	/// @param name          The name of the instance, just available with H_CREATED
	///
	void instanceStateChanged(InstanceState state, quint8 instance, const QString& name = QString());

	///
	/// @brief Emits whenever something changes, the lazy version of instanceStateChanged (- H_ON_STOP) + saveName() emit
	///
	void change();

	///
	/// @brief Emits when the user has requested to start a instance
	/// @param  caller  The origin caller instance who requested
	/// @param  tan     The tan that was part of the request
	///
	void startInstanceResponse(QObject* caller, const int& tan);

signals:
	///
	/// @brief PIPE settings events from HyperHDR
	///
	void settingsChanged(settings::type type, const QJsonDocument& data);

	///
	/// @brief PIPE videoMode request changes from HyperHDR to HyperHDRDaemon
	///
	void requestVideoModeHdr(int hdr);

	///
	/// @brief PIPE component state changes from HyperHDR to HyperHDRDaemon
	///
	void compStateChangeRequest(hyperhdr::Components component, bool enable);

	void setNewComponentStateToAllInstances(hyperhdr::Components component, bool enable);

private slots:
	///
	/// @brief handle started signal of HyperHDR instances
	///
	void handleStarted();

	///
	/// @brief handle finished signal of HyperHDR instances
	///
	void handleFinished();

private:
	friend class HyperHdrDaemon;
	///
	/// @brief Construct the Manager
	/// @param The root path of all userdata
	///
	HyperHdrIManager(const QString& rootPath, QObject* parent = nullptr, bool readonlyMode = false);

	///
	/// @brief Start all instances that are marked as enabled in db. Non blocking
	///
	void startAll();

	///
	/// @brief Stop all instances, used from HyperHDR deamon
	///
	void stopAll();

	///
	/// @brief check if a instance is allowed for management. Instance 0 represents the root instance
	/// @apram inst The instance to check
	///
	bool isInstAllowed(quint8 inst) const { return (inst > 0); }

private:
	Logger* _log;
	InstanceTable* _instanceTable;
	const QString	_rootPath;
	QMap<quint8, HyperHdrInstance*> _runningInstances;
	QList<quint8>	_startQueue;

	bool	_readonlyMode;
	int		_fireStarter;

	/// All pending requests
	QMap<quint8, PendingRequests> _pendingRequests;
};
