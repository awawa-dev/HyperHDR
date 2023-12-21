#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_PULSEWHITE "Music: fullscreen pulse (WHITE)"

class Animation4Music_PulseWhite : public AnimationBaseMusic
{
public:

	Animation4Music_PulseWhite();

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int 	 _oldMulti;
};
