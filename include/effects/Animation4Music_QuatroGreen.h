#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_QUATROGREEN "Music: quatro for LED strip (GREEN)"

class Animation4Music_QuatroGreen : public AnimationBaseMusic
{
public:

	Animation4Music_QuatroGreen();

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
