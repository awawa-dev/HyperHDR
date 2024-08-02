#pragma once

#include <effects/AnimationBase.h>

#define ANIM_PLASMA "Plasma"
#define PLASMA_WIDTH  80
#define PLASMA_HEIGHT 45
#define PAL_LEN       360
class Animation_Plasma : public AnimationBase
{
public:

	Animation_Plasma();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;

	static EffectDefinition getDefinition();

private:

	long long  start;

	int     plasma[PLASMA_WIDTH * PLASMA_HEIGHT];
	uint8_t pal[PAL_LEN * 3];

private:
	static bool isRegistered;

};
