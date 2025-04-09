#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_STEREOYELLOW "Music: stereo for LED strip (YELLOW)"

class Animation4Music_StereoYellow : public AnimationBaseMusic
{
public:

	Animation4Music_StereoYellow();

	bool Play(HyperImage& painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int 	 _oldMulti;

private:
	static bool isRegistered;

};
