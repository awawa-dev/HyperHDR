#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_PULSEMULTIFAST "Music: fullscreen pulse (MULTI COLOR FAST)"

class Animation4Music_PulseMultiFast : public AnimationBaseMusic
{
	Q_OBJECT	

public:
	
	Animation4Music_PulseMultiFast();

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
protected:
	
};
