#pragma once

#include <effectengine/Animation_Fade.h>

#define ANIM_BREATH "Breath"

class Animation_Breath : public Animation_Fade
{
public:

	Animation_Breath(QString name = ANIM_BREATH);

	static EffectDefinition getDefinition();
};
