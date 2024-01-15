#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_PULSEYELLOW "Music: fullscreen pulse (YELLOW)"

class Animation4Music_PulseYellow : public AnimationBaseMusic
{
public:

	Animation4Music_PulseYellow();

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
