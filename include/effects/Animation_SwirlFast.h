#pragma once

#include <effects/Animation_Swirl.h>

#define ANIM_SWIRL_FAST "Rainbow swirl fast"

class Animation_SwirlFast : public Animation_Swirl
{
public:

	Animation_SwirlFast();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
