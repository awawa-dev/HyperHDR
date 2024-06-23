#pragma once

#include <effects/Animation_Fade.h>

#define ANIM_CINEMA_DIM_LIGHTS "Cinema dim lights"

class Animation_CinemaDimLights : public Animation_Fade
{
public:

	Animation_CinemaDimLights();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
