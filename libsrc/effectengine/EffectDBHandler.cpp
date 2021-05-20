#include <effectengine/EffectDBHandler.h>


#include <effectengine/Animation_AtomicSwirl.h>
#include <effectengine/Animation_BlueMoodBlobs.h>
#include <effectengine/Animation_Breath.h>
#include <effectengine/Animation_Candle.h>
#include <effectengine/Animation_CinemaBrightenLights.h>
#include <effectengine/Animation_CinemaDimLights.h>
#include <effectengine/Animation_ColdMoodBlobs.h>
#include <effectengine/Animation_DoubleSwirl.h>
#include <effectengine/Animation_FullColorMoodBlobs.h>
#include <effectengine/Animation_GreenMoodBlobs.h>
#include <effectengine/Animation_KnightRider.h>
#include <effectengine/Animation_NotifyBlue.h>
#include <effectengine/Animation_Plasma.h>
#include <effectengine/Animation_PoliceLightsSolid.h>
#include <effectengine/Animation_PoliceLightsSingle.h>
#include <effectengine/Animation_RedMoodBlobs.h>
#include <effectengine/Animation_RainbowSwirl.h>
#include <effectengine/Animation_SeaWaves.h>
#include <effectengine/Animation_Sparks.h>
#include <effectengine/Animation_StrobeRed.h>
#include <effectengine/Animation_StrobeWhite.h>
#include <effectengine/Animation_SwirlFast.h>
#include <effectengine/Animation_SystemShutdown.h>
#include <effectengine/Animation_WarmMoodBlobs.h>
#include <effectengine/Animation_WavesWithColor.h>
#include <effectengine/Animation4Music_TestEq.h>
#include <effectengine/Animation4Music_PulseWhite.h>
#include <effectengine/Animation4Music_PulseBlue.h>
#include <effectengine/Animation4Music_PulseGreen.h>
#include <effectengine/Animation4Music_PulseRed.h>
#include <effectengine/Animation4Music_PulseYellow.h>
#include <effectengine/Animation4Music_PulseMulti.h>
#include <effectengine/Animation4Music_PulseMultiFast.h>
#include <effectengine/Animation4Music_PulseMultiSlow.h>
#include <effectengine/Animation4Music_StereoWhite.h>
#include <effectengine/Animation4Music_StereoBlue.h>
#include <effectengine/Animation4Music_StereoGreen.h>
#include <effectengine/Animation4Music_StereoRed.h>
#include <effectengine/Animation4Music_StereoYellow.h>
#include <effectengine/Animation4Music_StereoMulti.h>
#include <effectengine/Animation4Music_StereoMultiFast.h>
#include <effectengine/Animation4Music_StereoMultiSlow.h>
#include <effectengine/Animation4Music_QuatroWhite.h>
#include <effectengine/Animation4Music_QuatroBlue.h>
#include <effectengine/Animation4Music_QuatroGreen.h>
#include <effectengine/Animation4Music_QuatroRed.h>
#include <effectengine/Animation4Music_QuatroYellow.h>
#include <effectengine/Animation4Music_QuatroMulti.h>
#include <effectengine/Animation4Music_QuatroMultiFast.h>
#include <effectengine/Animation4Music_QuatroMultiSlow.h>

#include <effectengine/Animation4Music_WavesPulse.h>
#include <effectengine/Animation4Music_WavesPulseFast.h>
#include <effectengine/Animation4Music_WavesPulseSlow.h>

// util
#include <utils/JsonUtils.h>

// qt
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QByteArray>

EffectDBHandler* EffectDBHandler::efhInstance = NULL;

EffectDBHandler::EffectDBHandler(const QString& rootPath, const QJsonDocument& effectConfig, QObject* parent)
	: QObject(parent)
	, _effectConfig()
	, _log(Logger::getInstance("EFFECTDB"))
	, _rootPath(rootPath)
{
	EffectDBHandler::efhInstance = this;

	// init
	handleSettingsUpdate(settings::type::EFFECTS, effectConfig);
}

void EffectDBHandler::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::type::EFFECTS)
	{
		_effectConfig = config.object();
		updateEffects();
	}
}


void EffectDBHandler::updateEffects()
{
	_availableEffects.clear();

	_availableEffects.push_back(Animation4Music_WavesPulse::getDefinition());

	_availableEffects.push_back(Animation4Music_WavesPulseFast::getDefinition());

	_availableEffects.push_back(Animation4Music_WavesPulseSlow::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseMulti::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseMultiFast::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseMultiSlow::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseYellow::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseWhite::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseRed::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseGreen::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseBlue::getDefinition());


	_availableEffects.push_back(Animation4Music_StereoMulti::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoMultiFast::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoMultiSlow::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoYellow::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoWhite::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoRed::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoGreen::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoBlue::getDefinition());


	_availableEffects.push_back(Animation4Music_QuatroMulti::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroMultiFast::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroMultiSlow::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroYellow::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroWhite::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroRed::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroGreen::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroBlue::getDefinition());
	



	_availableEffects.push_back(Animation4Music_TestEq::getDefinition());

	_availableEffects.push_back(Animation_AtomicSwirl::getDefinition());

	_availableEffects.push_back(Animation_BlueMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_Breath::getDefinition());

	_availableEffects.push_back(Animation_Candle::getDefinition());

	_availableEffects.push_back(Animation_CinemaBrightenLights::getDefinition());

	_availableEffects.push_back(Animation_CinemaDimLights::getDefinition());

	_availableEffects.push_back(Animation_ColdMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_DoubleSwirl::getDefinition());

	_availableEffects.push_back(Animation_FullColorMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_GreenMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_KnightRider::getDefinition());

	_availableEffects.push_back(Animation_NotifyBlue::getDefinition());

	_availableEffects.push_back(Animation_Plasma::getDefinition());

	_availableEffects.push_back(Animation_PoliceLightsSingle::getDefinition());

	_availableEffects.push_back(Animation_PoliceLightsSolid::getDefinition());

	_availableEffects.push_back(Animation_RainbowSwirl::getDefinition());

	_availableEffects.push_back(Animation_SwirlFast::getDefinition());

	_availableEffects.push_back(Animation_RedMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_SeaWaves::getDefinition());

	_availableEffects.push_back(Animation_Sparks::getDefinition());

	_availableEffects.push_back(Animation_StrobeRed::getDefinition());
	
	_availableEffects.push_back(Animation_StrobeWhite::getDefinition());

	_availableEffects.push_back(Animation_SystemShutdown::getDefinition());

	_availableEffects.push_back(Animation_WarmMoodBlobs::getDefinition());
	
	_availableEffects.push_back(Animation_WavesWithColor::getDefinition());

	ErrorIf(_availableEffects.size()==0, _log, "No effects found... something gone wrong");

	emit effectListChanged();
}

EffectDBHandler* EffectDBHandler::getInstance()
{
	return efhInstance;
}

std::list<EffectDefinition> EffectDBHandler::getEffects() const
{
	return _availableEffects;
}
