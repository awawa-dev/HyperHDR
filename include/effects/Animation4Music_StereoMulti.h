#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_STEREOMULTI "Music: stereo for LED strip (MULTI COLOR)"

class Animation4Music_StereoMulti : public AnimationBaseMusic
{
public:

	Animation4Music_StereoMulti();

	bool Play(HyperImage& painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int		 _oldMulti;

private:
	static bool isRegistered;

};
