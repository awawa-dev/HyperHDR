#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_STEREOMULTIFAST "Music: stereo for LED strip (MULTI COLOR FAST)"

class Animation4Music_StereoMultiFast : public AnimationBaseMusic
{
public:

	Animation4Music_StereoMultiFast();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int		 _oldMulti;
};
