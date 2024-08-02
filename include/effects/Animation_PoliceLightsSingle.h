#pragma once

#include <effects/Animation_Police.h>

#define ANIM_POLICELIGHTSSINGLE "Police Lights Single"

class Animation_PoliceLightsSingle : public Animation_Police
{
public:

	Animation_PoliceLightsSingle();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
