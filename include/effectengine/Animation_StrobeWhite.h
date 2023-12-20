#pragma once

#include <effectengine/Animation_Fade.h>

#define ANIM_STROBE_WHITE "Strobe white"

class Animation_StrobeWhite : public Animation_Fade
{
public:
	Animation_StrobeWhite(QString name = ANIM_STROBE_WHITE);

	static EffectDefinition getDefinition();
};
