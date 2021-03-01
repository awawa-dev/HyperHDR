#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_QUATROWHITE "Music: quatro for LED strip (WHITE)"

class Animation4Music_QuatroWhite : public AnimationBaseMusic
{
	Q_OBJECT	

public:
	
	Animation4Music_QuatroWhite();

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
