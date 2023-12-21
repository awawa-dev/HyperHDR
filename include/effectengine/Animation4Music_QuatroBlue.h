#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_QUATROBLUE "Music: quatro for LED strip (BLUE)"

class Animation4Music_QuatroBlue : public AnimationBaseMusic
{
public:

	Animation4Music_QuatroBlue();

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
