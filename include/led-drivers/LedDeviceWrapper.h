#pragma once

#ifndef PCH_ENABLED
	#include <QMutex>
#endif

// util
#include <utils/Logger.h>
#include <image/ColorRgb.h>
#include <utils/Components.h>

class LedDevice;
class HyperHdrInstance;

typedef LedDevice* (*LedDeviceCreateFuncType) (const QJsonObject&);
typedef std::map<QString, LedDeviceCreateFuncType> LedDeviceRegistry;

class LedDeviceWrapper : public QObject
{
	Q_OBJECT
public:
	explicit LedDeviceWrapper(HyperHdrInstance* ownerInstance);
	virtual ~LedDeviceWrapper();

	void createLedDevice(QJsonObject config, int smoothingInterval, bool disableOnStartup);
	static QJsonObject getLedDeviceSchemas();
	QString getActiveDeviceType() const;
	bool enabled() const;
	unsigned int getLedCount() const;
	void identifyLed(const QJsonObject& params);
	int hasLedClock();

public slots:
	void handleComponentState(hyperhdr::Components component, bool state);
	void handleInternalEnableState(bool newState);

private:
	HyperHdrInstance* _ownerInstance;
	std::unique_ptr<LedDevice, void(*)(LedDevice*)> _ledDevice;
	bool              _enabled;
};
