#pragma once

#include <effects/AnimationBase.h>

#define ANIM_RAINBOW_WAVES "Rainbow waves"

class Animation_RainbowWaves : public AnimationBase
{
public:

	Animation_RainbowWaves();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(std::vector<ColorRgb>& buffer) override;

	static EffectDefinition getDefinition();

protected:

	int		hue;

private:
	static bool isRegistered;

};
