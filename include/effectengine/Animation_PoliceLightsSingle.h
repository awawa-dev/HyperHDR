#pragma once

#include <effectengine/Animation_Police.h>

#define ANIM_POLICELIGHTSSINGLE "Police Lights Single"

class Animation_PoliceLightsSingle : public Animation_Police
{
public:

	Animation_PoliceLightsSingle(QString name = ANIM_POLICELIGHTSSINGLE);

	static EffectDefinition getDefinition();
};
