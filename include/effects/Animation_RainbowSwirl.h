#pragma once

#include <effects/Animation_Swirl.h>

#define ANIM_RAINBOW_SWIRL "Rainbow swirl"

class Animation_RainbowSwirl : public Animation_Swirl
{
public:
	Animation_RainbowSwirl();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
