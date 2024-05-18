#pragma once

#include <effectengine/AnimationBase.h>

#define ANIM_SPARKS "Sparks"

class Animation_Sparks : public AnimationBase
{
public:
	Animation_Sparks();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(QVector<ColorRgb>& buffer) override;

	static EffectDefinition getDefinition();

protected:
	double     sleep_time;
	Point3d    color;

	QVector<ColorRgb> ledData;
};
