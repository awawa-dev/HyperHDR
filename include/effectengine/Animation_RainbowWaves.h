#pragma once

#include <effectengine/AnimationBase.h>

#define ANIM_RAINBOW_WAVES "Rainbow waves"

class Animation_RainbowWaves : public AnimationBase
{
public:

	Animation_RainbowWaves(QString name = ANIM_RAINBOW_WAVES);

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(QVector<ColorRgb>& buffer) override;

	static EffectDefinition getDefinition();

protected:

	int		hue;
};
