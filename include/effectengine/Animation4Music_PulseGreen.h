#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_PULSEGREEN "Music: fullscreen pulse (GREEN)"

class Animation4Music_PulseGreen : public AnimationBaseMusic
{
public:

	Animation4Music_PulseGreen();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int 	 _oldMulti;
};
