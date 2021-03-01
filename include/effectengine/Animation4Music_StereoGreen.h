#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_STEREOGREEN "Music: stereo for LED strip (GREEN)"

class Animation4Music_StereoGreen : public AnimationBaseMusic
{
	Q_OBJECT	

public:
	
	Animation4Music_StereoGreen();

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
