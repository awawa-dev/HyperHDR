#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_QUATROWHITE "Music: quatro for LED strip (WHITE)"

class Animation4Music_QuatroWhite : public AnimationBaseMusic
{
public:

	Animation4Music_QuatroWhite();

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
