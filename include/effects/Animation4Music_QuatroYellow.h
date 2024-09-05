#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_QUATROYELLOW "Music: quatro for LED strip (YELLOW)"

class Animation4Music_QuatroYellow : public AnimationBaseMusic
{
public:

	Animation4Music_QuatroYellow();

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
