#pragma once

#include <effects/AnimationBaseMusic.h>

#define AMUSIC_WAVESPULSE "Music: pulse waves for LED strip (MULTI COLOR)"

class Animation4Music_WavesPulse : public AnimationBaseMusic
{
public:

	Animation4Music_WavesPulse();

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
