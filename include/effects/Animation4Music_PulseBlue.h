#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_PULSEBLUE "Music: fullscreen pulse (BLUE)"

class Animation4Music_PulseBlue : public AnimationBaseMusic
{
public:

	Animation4Music_PulseBlue();

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
