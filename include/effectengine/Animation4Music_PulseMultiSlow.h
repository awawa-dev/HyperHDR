#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_PULSEMULTISLOW "Music: fullscreen pulse (MULTI COLOR SLOW)"

class Animation4Music_PulseMultiSlow : public AnimationBaseMusic
{
	Q_OBJECT	

public:
	
	Animation4Music_PulseMultiSlow();

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
protected:
	
};
