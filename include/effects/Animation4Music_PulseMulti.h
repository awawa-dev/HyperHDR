#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_PULSEMULTI "Music: fullscreen pulse (MULTI COLOR)"

class Animation4Music_PulseMulti : public AnimationBaseMusic
{
public:

	Animation4Music_PulseMulti();

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
