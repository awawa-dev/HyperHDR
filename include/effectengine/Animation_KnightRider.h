#pragma once

#include <effectengine/AnimationBase.h>

#define ANIM_KNIGHT_RIDER "Knight rider"
#define KNIGHT_WIDTH  25
#define KNIGHT_HEIGHT 25

class Animation_KnightRider : public AnimationBase
{
	Q_OBJECT

private:

	static QJsonObject GetArgs();

public:

	Animation_KnightRider();

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;

	static EffectDefinition getDefinition();

private:

	double speed;
	double fadeFactor;
	Point3d color;
	int increment;
	int position;
	int direction;

	uint8_t imageData[KNIGHT_WIDTH * 3];
};
