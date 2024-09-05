#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <QList>
#endif

#include <utils/settings.h>
#include <utils/Components.h>
#include <api/BaseAPI.h>
#include <base/AccessManager.h>
#include <lut-calibrator/LutCalibrator.h>

#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>
#endif


class HyperHdrInstance;
class ComponentRegister;
class BonjourBrowserWrapper;
class PriorityMuxer;
class LutCalibrator;

class CallbackAPI : public BaseAPI
{
	Q_OBJECT

public:
	CallbackAPI(Logger* log, bool localConnection, QObject* parent);

	void subscribe(QJsonArray subsArr);

protected:
	std::unique_ptr<LutCalibrator> _lutCalibrator;
	Image<ColorRgb> _liveImage;

	void stopDataConnections() override = 0;
	void removeSubscriptions() override;
	void addSubscriptions() override;
	bool subscribeFor(const QString& cmd, bool unsubscribe = false);

signals:
	void SignalCallbackToClient(QJsonObject);

protected slots:
	virtual void handleIncomingColors(const std::vector<ColorRgb>& ledValues) = 0;
	virtual void handlerInstanceImageUpdated(const Image<ColorRgb>& image) = 0;

private slots:
	void componentStateHandler(hyperhdr::Components comp, bool state);
	void priorityUpdateHandler();
	void imageToLedsMappingChangeHandler(int mappingType);
	void signalAdjustmentUpdatedHandler(const QJsonArray& newConfig);
	void videoModeHdrChangeHandler(hyperhdr::Components component, bool enable);
	void videoStreamChangedHandler(QString device, QString videoMode);
	void settingsChangeHandler(settings::type type, const QJsonDocument& data);
	void ledsConfigChangeHandler(settings::type type, const QJsonDocument& data);
	void instancesListChangedHandler();
	void tokenChangeHandler(const QVector<AccessManager::AuthDefinition>& def);
	void signalBenchmarkUpdateHandler(int status, QString message);
	void lutCalibrationUpdateHandler(const QJsonObject& data);
	void performanceUpdateHandler(const QJsonObject& data);
#ifdef ENABLE_BONJOUR
	void signalDiscoveryFoundServiceHandler(DiscoveryRecord::Service type, QList<DiscoveryRecord> records);
#endif

private:
	QStringList _availableCommands;
	QStringList _subscribedCommands;
	void doCallback(const QString& cmd, const QVariant& data);	
};
