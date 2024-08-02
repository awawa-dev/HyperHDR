#pragma once

#include <effects/Animation_Waves.h>

#define ANIM_WAVESWITHCOLOR "Waves with Color"

class Animation_WavesWithColor : public Animation_Waves
{
public:
	Animation_WavesWithColor();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
