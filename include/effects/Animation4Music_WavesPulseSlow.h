#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_WAVESPULSESLOW "Music: pulse waves for LED strip (MULTI COLOR SLOW)"

class Animation4Music_WavesPulseSlow : public AnimationBaseMusic
{
public:

	Animation4Music_WavesPulseSlow();

	bool Play(HyperImage& painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int 	 _oldMulti;
	std::vector<ColorRgb> _buffer;

private:
	static bool isRegistered;

};
