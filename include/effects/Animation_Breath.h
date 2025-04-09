#pragma once

#include <effects/Animation_Fade.h>

#define ANIM_BREATH "Breath"

class Animation_Breath : public Animation_Fade
{
public:

	Animation_Breath();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
