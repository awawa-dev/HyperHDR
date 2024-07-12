#pragma once

#include <effects/AnimationBase.h>

class Animation_CandleLight : public AnimationBase
{
public:

	Animation_CandleLight();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(std::vector<ColorRgb>& buffer) override;

private:

	Point3d	   CandleRgb();

protected:

	int        colorShift;
	Point3d    color;
	Point3dhsv hsv;

	std::vector<ColorRgb> ledData;
};
