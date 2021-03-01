#pragma once

#include <utils/Logger.h>
#include <hyperhdrbase/HyperHdrInstance.h>
#include <utils/settings.h>

///
/// @brief Handle the background Effect settings, reacts on runtime to settings changes
/// 
class BGEffectHandler : public QObject
{
	Q_OBJECT

public:
	BGEffectHandler(HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr)
	, _hyperhdr(hyperhdr)
	{
		// listen for config changes
		connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &BGEffectHandler::handleSettingsUpdate);

		// initialization
		handleSettingsUpdate(settings::type::BGEFFECT, _hyperhdr->getSetting(settings::type::BGEFFECT));
	}

private slots:
	///
	/// @brief Handle settings update from HyperHdr Settingsmanager emit or this constructor
	/// @param type   settingType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config)
	{
		if(type == settings::type::BGEFFECT)
		{
			const QJsonObject& BGEffectConfig = config.object();

			#define BGCONFIG_ARRAY bgColorConfig.toArray()
			// clear background priority
			_hyperhdr->clear(254);
			// initial background effect/color
			if (BGEffectConfig["enable"].toBool(true))
			{
				const QString bgTypeConfig = BGEffectConfig["type"].toString("effect");
				const QString bgEffectConfig = BGEffectConfig["effect"].toString("Warm mood blobs");
				const QJsonValue bgColorConfig = BGEffectConfig["color"];
				if (bgTypeConfig.contains("color"))
				{
					std::vector<ColorRgb> bg_color = {
						ColorRgb {
							static_cast<uint8_t>(BGCONFIG_ARRAY.at(0).toInt(0)),
							static_cast<uint8_t>(BGCONFIG_ARRAY.at(1).toInt(0)),
							static_cast<uint8_t>(BGCONFIG_ARRAY.at(2).toInt(0))
						}
					};
					_hyperhdr->setColor(254, bg_color);
					Info(Logger::getInstance("HYPERHDR"),"Initial background color set (%d %d %d)",bg_color.at(0).red, bg_color.at(0).green, bg_color.at(0).blue);
				}
				else
				{
					int result = _hyperhdr->setEffect(bgEffectConfig, 254);
					Info(Logger::getInstance("HYPERHDR"),"Initial background effect '%s' %s", QSTRING_CSTR(bgEffectConfig), ((result == 0) ? "started" : "failed"));
				}
			}

			#undef BGCONFIG_ARRAY
		}
	}

private:
	HyperHdrInstance* _hyperhdr;
};
