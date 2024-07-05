#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_PULSEMULTISLOW "Music: fullscreen pulse (MULTI COLOR SLOW)"

class Animation4Music_PulseMultiSlow : public AnimationBaseMusic
{
public:

	Animation4Music_PulseMultiSlow();

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
