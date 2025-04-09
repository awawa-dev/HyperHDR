#pragma once

#include <effects/Animation_Waves.h>

#define ANIM_SEAWAVES "Sea waves"

class Animation_SeaWaves : public Animation_Waves
{
public:

	Animation_SeaWaves();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
