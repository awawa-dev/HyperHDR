#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_PULSERED "Music: fullscreen pulse (RED)"

class Animation4Music_PulseRed : public AnimationBaseMusic
{
	Q_OBJECT	

public:
	
	Animation4Music_PulseRed();

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
