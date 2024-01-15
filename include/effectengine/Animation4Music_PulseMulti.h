#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_PULSEMULTI "Music: fullscreen pulse (MULTI COLOR)"

class Animation4Music_PulseMulti : public AnimationBaseMusic
{
public:

	Animation4Music_PulseMulti();

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
