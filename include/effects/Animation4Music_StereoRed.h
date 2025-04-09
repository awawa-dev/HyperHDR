#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_STEREORED "Music: stereo for LED strip (RED)"

class Animation4Music_StereoRed : public AnimationBaseMusic
{
public:

	Animation4Music_StereoRed();

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
