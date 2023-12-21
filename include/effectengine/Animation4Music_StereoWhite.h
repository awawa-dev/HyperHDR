#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_STEREOWHITE "Music: stereo for LED strip (WHITE)"

class Animation4Music_StereoWhite : public AnimationBaseMusic
{
public:

	Animation4Music_StereoWhite();

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
