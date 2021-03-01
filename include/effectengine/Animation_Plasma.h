#pragma once

#include <effectengine/AnimationBase.h>

#define ANIM_PLASMA "Plasma"
#define PLASMA_WIDTH  80
#define PLASMA_HEIGHT 45
#define PAL_LEN       360
class Animation_Plasma : public AnimationBase
{
	Q_OBJECT

	static QJsonObject GetArgs();

public:
	
	Animation_Plasma();

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;	

	static EffectDefinition getDefinition();

private:

	qint64  start;

	int     plasma[PLASMA_WIDTH*PLASMA_HEIGHT];
	uint8_t pal[PAL_LEN*3];
protected:
	
};
