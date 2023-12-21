#pragma once

#include <effectengine/AnimationBase.h>
class Animation_MoodBlobs : public AnimationBase
{
public:
	Animation_MoodBlobs(QString name);

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;
	bool hasLedData(QVector<ColorRgb>& buffer) override;

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

	QVector<Point3d> colorData;

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
