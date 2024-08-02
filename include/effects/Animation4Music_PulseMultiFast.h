#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_PULSEMULTIFAST "Music: fullscreen pulse (MULTI COLOR FAST)"

class Animation4Music_PulseMultiFast : public AnimationBaseMusic
{
public:

	Animation4Music_PulseMultiFast();

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
