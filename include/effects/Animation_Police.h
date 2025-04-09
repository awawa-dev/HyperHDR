#pragma once

#include <effects/AnimationBase.h>

class Animation_Police : public AnimationBase
{
public:

	Animation_Police();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(std::vector<ColorRgb>& buffer) override;

protected:

	double  rotationTime;
	Point3d colorOne;
	Point3d colorTwo;
	bool    reverse;
	int		colorsCount;
	int		increment;
	std::vector<ColorRgb> ledData;
};
