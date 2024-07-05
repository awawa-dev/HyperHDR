#pragma once

#include <effects/Animation_Fade.h>

#define ANIM_STROBE_WHITE "Strobe white"

class Animation_StrobeWhite : public Animation_Fade
{
public:
	Animation_StrobeWhite();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
