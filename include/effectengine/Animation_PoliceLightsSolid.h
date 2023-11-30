#pragma once

#include <effectengine/Animation_Police.h>

#define ANIM_POLICELIGHTSSOLID "Police Lights Solid"

class Animation_PoliceLightsSolid : public Animation_Police
{
public:

	Animation_PoliceLightsSolid(QString name = ANIM_POLICELIGHTSSOLID);

	static EffectDefinition getDefinition();
};
