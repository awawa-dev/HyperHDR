#pragma once

#include <utils/Logger.h>
#include <base/HyperHdrInstance.h>
#include <utils/settings.h>

///
/// @brief Handle the background Effect settings, reacts on runtime to settings changes
/// 
class BGEffectHandler : public QObject
{
	Q_OBJECT

public:
	BGEffectHandler(HyperHdrInstance* hyperhdr);

private slots:
	///
	/// @brief Handle settings update from HyperHdr Settingsmanager emit or this constructor
	/// @param type   settingType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

private:
	HyperHdrInstance* _hyperhdr;
};
