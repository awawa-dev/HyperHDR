#pragma once

#include <effectengine/AnimationBase.h>

class Animation_CandleLight : public AnimationBase
{
public:

	Animation_CandleLight(QString name);

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(QVector<ColorRgb>& buffer) override;

private:

	Point3d	   CandleRgb();

protected:

	int        colorShift;
	Point3d    color;
	Point3dhsv hsv;

	QVector<ColorRgb> ledData;
};
