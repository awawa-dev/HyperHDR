#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_PULSEGREEN "Music: fullscreen pulse (GREEN)"

class Animation4Music_PulseGreen : public AnimationBaseMusic
{
public:

	Animation4Music_PulseGreen();

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
