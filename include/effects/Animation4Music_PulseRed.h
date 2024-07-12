#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_PULSERED "Music: fullscreen pulse (RED)"

class Animation4Music_PulseRed : public AnimationBaseMusic
{
public:

	Animation4Music_PulseRed();

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
