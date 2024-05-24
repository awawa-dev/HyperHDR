#pragma once

#include <effectengine/AnimationBase.h>

class Animation_Police : public AnimationBase
{
public:

	Animation_Police(QString name);

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;
	bool hasLedData(QVector<ColorRgb>& buffer) override;

protected:

	double  rotationTime;
	Point3d colorOne;
	Point3d colorTwo;
	bool    reverse;
	int		colorsCount;
	int		increment;
	QVector<ColorRgb> ledData;
};
