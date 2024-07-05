#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_QUATROBLUE "Music: quatro for LED strip (BLUE)"

class Animation4Music_QuatroBlue : public AnimationBaseMusic
{
public:

	Animation4Music_QuatroBlue();

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
