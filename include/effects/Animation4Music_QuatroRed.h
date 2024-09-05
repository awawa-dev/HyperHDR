#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_QUATRORED "Music: quatro for LED strip (RED)"

class Animation4Music_QuatroRed : public AnimationBaseMusic
{
public:

	Animation4Music_QuatroRed();

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
