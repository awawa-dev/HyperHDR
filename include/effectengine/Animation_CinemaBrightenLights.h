#pragma once

#include <effectengine/Animation_Fade.h>

#define ANIM_CINEMA_BRIGHTEN_LIGHTS "Cinema brighten lights"

class Animation_CinemaBrightenLights : public Animation_Fade
{
	Q_OBJECT

	static QJsonObject GetArgs();

public:
	Animation_CinemaBrightenLights(QString name=ANIM_CINEMA_BRIGHTEN_LIGHTS);

	static EffectDefinition getDefinition();
};
