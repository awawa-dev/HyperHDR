#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_TESTEQ "Music: equalizer test (turn on video preview)"

class Animation4Music_TestEq : public AnimationBaseMusic
{
public:

	Animation4Music_TestEq();

	bool Play(HyperImage& painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int		 _oldMulti;

private:
	static bool isRegistered;

};
