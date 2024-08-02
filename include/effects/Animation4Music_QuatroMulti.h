#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_QUATROMULTI "Music: quatro for LED strip (MULTI COLOR)"

class Animation4Music_QuatroMulti : public AnimationBaseMusic
{
public:

	Animation4Music_QuatroMulti();

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
