#pragma once

// qt incl
#include <QJsonObject>
#include <QList>

#include <utils/Components.h>
#include <utils/settings.h>
#include <base/AuthManager.h>

#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>
#endif


class HyperHdrInstance;
class ComponentRegister;
class BonjourBrowserWrapper;
class PriorityMuxer;

class JsonCB : public QObject
{
	Q_OBJECT

public:
	JsonCB(QObject* parent);

	///
	/// @brief Subscribe to future data updates given by cmd
	/// @param cmd          The cmd which will be subscribed for
	/// @param unsubscribe  Revert subscription
	/// @return      True on success, false if not found
	///
	bool subscribeFor(const QString& cmd, bool unsubscribe = false);

	///
	/// @brief Get all possible commands to subscribe for
	/// @return  The list of commands
	///
	QStringList getCommands();

	///
	/// @brief Get all subscribed commands
	/// @return  The list of commands
	///
	QStringList getSubscribedCommands();

	///
	/// @brief Reset subscriptions, disconnect all signals
	///
	void resetSubscriptions();

	///
	/// @brief Re-apply all current subs to a new HyperHDR instance, the connections to the old instance will be dropped
	///
	void setSubscriptionsTo(HyperHdrInstance* hyperhdr);

	void handleLutInstallUpdate(const QJsonObject& data);

signals:
	///
	/// @brief Emits whenever a new json mesage callback is ready to send
	/// @param The JsonObject message
	///
	void newCallback(QJsonObject);

private slots:
	///
	/// @brief handle component state changes
	///
	void handleComponentState(hyperhdr::Components comp, bool state);

#ifdef ENABLE_BONJOUR
	void handleNetworkDiscoveryChange(DiscoveryRecord::Service type, QList<DiscoveryRecord> records);
#endif

	///
	/// @brief handle emits from PriorityMuxer
	///
	void handlePriorityUpdate();

	///
	/// @brief Handle imageToLedsMapping updates
	///
	void handleImageToLedsMappingChange(int mappingType);

	///
	/// @brief Handle the adjustment update
	///
	void handleAdjustmentChange();

	///
	/// @brief Handle video mode change
	/// @param mode  The new videoMode
	///
	void handleVideoModeHdrChange(int hdr);

	void handleGrabberStateChange(QString device, QString videoMode);

	///
	/// @brief Handle a config part change. This does NOT include (global) changes from other hyperhdr instances
	/// @param type   The settings type from enum
	/// @param data   The data as QJsonDocument
	///
	void handleSettingsChange(settings::type type, const QJsonDocument& data);

	///
	/// @brief Handle led config specific updates (required for led color streaming with positional display)
	/// @param type   The settings type from enum
	/// @param data   The data as QJsonDocument
	///
	void handleLedsConfigChange(settings::type type, const QJsonDocument& data);

	///
	/// @brief Handle HyperHDR instance manager change
	///
	void handleInstanceChange();

	///
	/// @brief Handle AuthManager token changes
	///
	void handleTokenChange(const QVector<AuthManager::AuthDefinition>& def);

	void handleBenchmarkUpdate(int status, QString message);

	void handleLutCalibrationUpdate(const QJsonObject& data);

	void handlePerformanceUpdate(const QJsonObject& data);

private:
	/// pointer of HyperHDR instance
	HyperHdrInstance* _hyperhdr;
	/// contains all available commands
	QStringList _availableCommands;
	/// contains active subscriptions
	QStringList _subscribedCommands;
	/// construct callback msg
	void doCallback(const QString& cmd, const QVariant& data);
};
