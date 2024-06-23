#pragma once

#include <effects/AnimationBase.h>

class Animation_Swirl : public AnimationBase
{
public:

	struct SwirlGradient
	{
		uint8_t items[5];
	};

	struct SwirlColor
	{
		uint8_t items[4];
	};

	Animation_Swirl();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;

private:

	Point2d getPoint(const HyperImage& hyperImage, bool random, double x, double y);
	int   getSTime(int hyperLatchTime, int _rt, double steps);

	void  buildGradient(std::vector<Animation_Swirl::SwirlGradient>& ba, bool withAlpha, std::vector<Animation_Swirl::SwirlColor> cc, bool closeCircle);
	bool  imageConicalGradient(HyperImage& painter, int centerX, int centerY, int angle, const std::vector<Animation_Swirl::SwirlGradient>& bytearray, bool reset);

	Point2d pointS1;
	Point2d pointS2;

	int angle;
	int angle2;
	bool   S2;

	int increment;
	int increment2;
	std::vector<Animation_Swirl::SwirlGradient> baS1, baS2;


protected:

	double rotation_time;
	bool reverse;
	double center_x;
	double center_y;
	bool random_center;
	std::vector<Animation_Swirl::SwirlColor> custom_colors;
	bool enable_second;
	bool reverse2;
	double center_x2;
	double center_y2;
	bool random_center2;
	std::vector<Animation_Swirl::SwirlColor> custom_colors2;
};
