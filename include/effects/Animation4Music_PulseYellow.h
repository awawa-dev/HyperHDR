#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_PULSEYELLOW "Music: fullscreen pulse (YELLOW)"

class Animation4Music_PulseYellow : public AnimationBaseMusic
{
public:

	Animation4Music_PulseYellow();

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
