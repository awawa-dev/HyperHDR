#pragma once

#include <effects/Animation_Fade.h>

#define ANIM_STROBE_RED "Strobe red"

class Animation_StrobeRed : public Animation_Fade
{
public:
	Animation_StrobeRed();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
