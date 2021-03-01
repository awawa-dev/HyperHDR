#pragma once

#include <effectengine/AnimationBase.h>
#include <effectengine/Animation_Swirl.h>
class Animation_Waves : public AnimationBase
{
	Q_OBJECT

public:	
	Animation_Waves(QString name);

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;
private:
	Point2d getPoint(const QImage& hyperImage, bool random, double x, double y);
	int   getSTime(int hyperLatchTime, int _rt, double steps);
		
	void  buildGradient(QList<Animation_Swirl::SwirlGradient>& ba, bool withAlpha, QList<Animation_Swirl::SwirlColor> cc, bool closeCircle);
	bool  imageRadialGradient(QPainter* painter, int centerX, int centerY, int angle, const QList<Animation_Swirl::SwirlGradient>& bytearray);

	Point2d pointS1;

	
	QList<Animation_Swirl::SwirlGradient> baS1;
	int		       diag;
	QList<uint8_t> positions;
	qint64         targetTime;

protected:

	double rotation_time;
	bool reverse;
	double center_x;
	double center_y;
	bool random_center;
	int  reverse_time;
	
	QList<Animation_Swirl::SwirlColor> custom_colors;
};
