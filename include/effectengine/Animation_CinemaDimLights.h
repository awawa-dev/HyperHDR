#pragma once

#include <effectengine/Animation_Fade.h>

#define ANIM_CINEMA_DIM_LIGHTS "Cinema dim lights"

class Animation_CinemaDimLights : public Animation_Fade
{
public:

	Animation_CinemaDimLights(QString name = ANIM_CINEMA_DIM_LIGHTS);

	static EffectDefinition getDefinition();
};
