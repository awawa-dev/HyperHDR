#pragma once

#include <effects/Animation_Swirl.h>

#define ANIM_DOUBLE_SWIRL "Double swirl"

class Animation_DoubleSwirl : public Animation_Swirl
{
public:

	Animation_DoubleSwirl();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
