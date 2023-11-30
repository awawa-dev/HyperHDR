#pragma once

#include <effectengine/AnimationBase.h>
class Animation_Fade : public AnimationBase
{
public:

	Animation_Fade(QString name);

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;
	bool hasLedData(QVector<ColorRgb>& buffer) override;

private:

	void SetPoint(Point3d point);
	int     currentStep;
	Point3d current;
	int     incrementIn;
	double  sleepTimeIn;
	int     incrementOut;
	double  sleepTimeOut;
	int     repeatCounter;
	double  steps;
	QVector<Point3d> colors;

protected:

	double  fadeInTime;
	double  fadeOutTime;
	Point3d colorStart;
	Point3d colorEnd;
	double  colorStartTime;
	double  colorEndTime;
	int     repeat;
	bool    maintainEndCol;
	double  minStepTime;

	int     indexer1;
	double  indexer2;
	int     indexer3;
	double  indexer4;
	int     indexer6;
};
