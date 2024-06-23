#pragma once

#include <effects/Animation_Police.h>

#define ANIM_POLICELIGHTSSOLID "Police Lights Solid"

class Animation_PoliceLightsSolid : public Animation_Police
{
public:

	Animation_PoliceLightsSolid();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
