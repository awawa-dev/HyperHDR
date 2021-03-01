#pragma once

#include <effectengine/AnimationBaseMusic.h>
#include <QList>

#define AMUSIC_WAVESPULSE "Music: pulse waves for LED strip (MULTI COLOR)"

class Animation4Music_WavesPulse : public AnimationBaseMusic
{
	Q_OBJECT	

public:
	
	Animation4Music_WavesPulse();

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
	QList<QColor> _buffer;
protected:
	
};
