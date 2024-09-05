#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_STEREOBLUE "Music: stereo for LED strip (BLUE)"

class Animation4Music_StereoBlue : public AnimationBaseMusic
{
public:

	Animation4Music_StereoBlue();

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
