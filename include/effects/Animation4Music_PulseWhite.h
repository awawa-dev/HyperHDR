#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_PULSEWHITE "Music: fullscreen pulse (WHITE)"

class Animation4Music_PulseWhite : public AnimationBaseMusic
{
public:

	Animation4Music_PulseWhite();

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
