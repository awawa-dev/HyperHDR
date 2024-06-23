#pragma once

#include <effects/Animation_Fade.h>

#define ANIM_CINEMA_BRIGHTEN_LIGHTS "Cinema brighten lights"

class Animation_CinemaBrightenLights : public Animation_Fade
{
public:

	Animation_CinemaBrightenLights();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
