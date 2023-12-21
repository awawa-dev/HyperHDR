#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_QUATROMULTIFAST "Music: quatro for LED strip (MULTI COLOR FAST)"

class Animation4Music_QuatroMultiFast : public AnimationBaseMusic
{
public:

	Animation4Music_QuatroMultiFast();

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int		 _oldMulti;
};
