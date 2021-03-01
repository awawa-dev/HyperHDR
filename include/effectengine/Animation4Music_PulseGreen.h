#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_PULSEGREEN "Music: fullscreen pulse (GREEN)"

class Animation4Music_PulseGreen : public AnimationBaseMusic
{
	Q_OBJECT	

public:
	
	Animation4Music_PulseGreen();

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
