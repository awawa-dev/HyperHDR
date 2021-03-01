#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_TESTEQ "Music: equalizer test (turn on video preview)"

class Animation4Music_TestEq : public AnimationBaseMusic
{
	Q_OBJECT	

public:
	
	Animation4Music_TestEq();

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;
private:
	uint32_t _internalIndex;
protected:
	
};
