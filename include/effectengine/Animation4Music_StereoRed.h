#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_STEREORED "Music: stereo for LED strip (RED)"

class Animation4Music_StereoRed : public AnimationBaseMusic
{
public:

	Animation4Music_StereoRed();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int 	 _oldMulti;
};
