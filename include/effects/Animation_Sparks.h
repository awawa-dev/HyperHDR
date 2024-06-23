#pragma once

#include <effects/AnimationBase.h>

#define ANIM_SPARKS "Sparks"

class Animation_Sparks : public AnimationBase
{
public:
	Animation_Sparks();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(std::vector<ColorRgb>& buffer) override;

	static EffectDefinition getDefinition();

protected:
	double     sleep_time;
	Point3d    color;

	std::vector<ColorRgb> ledData;

private:
	static bool isRegistered;

};
