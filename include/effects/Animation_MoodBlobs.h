#pragma once

#include <effects/AnimationBase.h>
class Animation_MoodBlobs : public AnimationBase
{
public:
	Animation_MoodBlobs();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(std::vector<ColorRgb>& buffer) override;

private:
	int    hyperledCount;
	bool   fullColorWheelAvailable;
	double baseColorChangeIncreaseValue;
	double sleepTime;
	double amplitudePhaseIncrement;
	int    colorDataIncrement;
	double amplitudePhase;
	bool   rotateColors;
	int    baseColorChangeStepCount;
	double baseHSVValue;
	int    numberOfRotates;
	Point3dhsv baseHsv;

	std::vector<Point3d> colorData;

protected:
	double rotationTime;
	Point3d color;
	bool   colorRandom;
	double hueChange;
	int    blobs;
	bool   reverse;
	bool   baseColorChange;
	double baseColorRangeLeft;
	double baseColorRangeRight;
	double baseColorChangeRate;
};
