#pragma once

#include <effectengine/Animation_Swirl.h>

#define ANIM_RAINBOW_SWIRL "Rainbow swirl"

class Animation_RainbowSwirl : public Animation_Swirl
{
public:
	Animation_RainbowSwirl(QString name = ANIM_RAINBOW_SWIRL);

	static EffectDefinition getDefinition();
};
