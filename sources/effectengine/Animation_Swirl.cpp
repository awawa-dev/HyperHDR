/* Animation_Swirl.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <effectengine/Animation_Swirl.h>

Animation_Swirl::Animation_Swirl(QString name) :
	AnimationBase(name)
{
	rotation_time = 10.0;
	reverse = false;
	center_x = 0.5;
	center_y = 0.5;
	random_center = false;

	enable_second = false;
	reverse2 = true;
	center_x2 = 0.5;
	center_y2 = 0.5;
	random_center2 = false;


	pointS1 = Point2d({ 0, 0 });
	pointS2 = Point2d({ 0, 0 });
	SetSleepTime(100);
	angle = 0;
	angle2 = 0;
	S2 = false;;
	increment = 1;
	increment2 = -1;

	custom_colors.append({ 255, 0, 0, 0 });
	custom_colors.append({ 0, 255, 0, 0 });
	custom_colors.append({ 0, 0, 255, 0 });

	custom_colors2.append({ 255, 255, 255, 0 });
	custom_colors2.append({ 0, 255, 255, 0 });
	custom_colors2.append({ 255, 255, 255, 1 });
	custom_colors2.append({ 0, 255, 255, 0 });
	custom_colors2.append({ 0, 255, 255, 0 });
	custom_colors2.append({ 0, 255, 255, 0 });
	custom_colors2.append({ 255, 255, 255, 1 });
	custom_colors2.append({ 0, 255, 255, 0 });
	custom_colors2.append({ 0, 255, 255, 0 });
	custom_colors2.append({ 0, 255, 255, 0 });
	custom_colors2.append({ 255, 255, 255, 1 });
	custom_colors2.append({ 0, 255, 255, 0 });
};

Point2d Animation_Swirl::getPoint(const HyperImage& hyperImage, bool random, double x, double y) {
	Point2d p;

	if (random)
	{
		x = static_cast <double> (rand()) / static_cast <double> (RAND_MAX);
		y = static_cast <double> (rand()) / static_cast <double> (RAND_MAX);
	}

	p.x = int(round(x * hyperImage.width()));
	p.y = int(round(y * hyperImage.height()));

	return p;
}

int Animation_Swirl::getSTime(int hyperLatchTime, int _rt, double steps = 360) {

	double rt = double(_rt);
	double sleepTime = std::max(0.1, rt) / steps;


	double minStepTime = double(std::max(hyperLatchTime, 15)) / 1000.0;

	if (minStepTime < 0.001)
		minStepTime = 0.001;

	if (minStepTime > sleepTime)
		sleepTime = minStepTime;

	return int(round(sleepTime * 1000.0));
}

void Animation_Swirl::buildGradient(QList<Animation_Swirl::SwirlGradient>& ba, bool withAlpha, QList<Animation_Swirl::SwirlColor> cc, bool closeCircle = true)
{
	if (cc.length() > 1)
	{
		int posfac = int(255 / cc.length());
		int pos = 0;


		foreach(auto c, cc)
		{
			uint8_t alpha = 255;
			if (withAlpha && c.items[3] == 0)
				alpha = 0;

			pos += posfac;

			if (pos > 255)
				pos = 255;

			ba.append(Animation_Swirl::SwirlGradient({ uint8_t(pos), c.items[0], c.items[1], c.items[2], alpha }));
		}

		if (closeCircle)
		{
			uint8_t alpha = 255;
			auto lC = cc[cc.length() - 1];

			if (withAlpha && lC.items[3] == 0)
				alpha = 0;
			ba.append(Animation_Swirl::SwirlGradient({ 0, lC.items[0], lC.items[1], lC.items[2], alpha }));
		}
	}
}

void Animation_Swirl::Init(
	HyperImage& hyperImage,
	int hyperLatchTime
)
{
	QList<Animation_Swirl::SwirlColor> _custColors, _custColors2;

	hyperImage.resize(80, 45);

	auto _rotationTime = rotation_time;
	auto _reverse = reverse;
	auto _centerX = center_x;
	auto _centerY = center_y;
	auto _randomCenter = random_center;

	_custColors = custom_colors;

	auto _enableSecond = enable_second;
	auto _reverse2 = reverse2;
	auto _centerX2 = center_x2;
	auto _centerY2 = center_y2;
	auto _randomCenter2 = random_center2;


	_custColors2 = custom_colors2;

	pointS1 = getPoint(hyperImage, _randomCenter, _centerX, _centerY);
	pointS2 = getPoint(hyperImage, _randomCenter2, _centerX2, _centerY2);


	SetSleepTime(getSTime(hyperLatchTime, _rotationTime));

	angle = 0;
	angle2 = 0;
	S2 = false;

	increment = (_reverse) ? -1 : 1;
	increment2 = (_reverse2) ? -1 : 1;


	if (_custColors.length() > 1)
		buildGradient(baS1, false, _custColors);
	else
	{
		baS1.append({ 0, 255, 0, 0, 255 });
		baS1.append({ 25, 255, 230, 0, 255 });
		baS1.append({ 63, 255, 255, 0, 255 });
		baS1.append({ 100, 0, 255, 0, 255 });
		baS1.append({ 127, 0, 255, 200, 255 });
		baS1.append({ 159, 0, 255, 255, 255 });
		baS1.append({ 191, 0, 0, 255, 255 });
		baS1.append({ 224, 255, 0, 255, 255 });
		baS1.append({ 255, 255, 0, 127, 255 });
	}

	if (_enableSecond && _custColors2.length() > 1)
	{
		S2 = true;
		buildGradient(baS2, true, _custColors2);
	}
}

bool Animation_Swirl::Play(HyperImage& painter)
{
	bool ret;
	angle += increment;
	if (angle > 360)
		angle = 0;
	if (angle < 0)
		angle = 360;

	angle2 += increment2;
	if (angle2 > 360)
		angle2 = 0;
	if (angle2 < 0)
		angle2 = 360;

	ret = imageConicalGradient(painter, pointS1.x, pointS1.y, angle, baS1);
	if (S2)
		ret |= imageConicalGradient(painter, pointS2.x, pointS2.y, angle2, baS2);
	return ret;
}


bool Animation_Swirl::imageConicalGradient(HyperImage& painter, int centerX, int centerY, int angle, const QList<Animation_Swirl::SwirlGradient>& bytearray)
{
	int startX = 0;
	int startY = 0;
	int width = painter.width();
	int height = painter.height();

	angle = qMax(qMin(angle, 360), 0);


	QRect myQRect(startX, startY, width, height);
	QConicalGradient gradient(QPoint(centerX, centerY), angle);

	foreach(Animation_Swirl::SwirlGradient item, bytearray)
	{
		gradient.setColorAt(
			((uint8_t)item.items[0]) / 255.0,
			QColor(
				(uint8_t)(item.items[1]),
				(uint8_t)(item.items[2]),
				(uint8_t)(item.items[3]),
				(uint8_t)(item.items[4])
			));
	}

	painter->fillRect(myQRect, gradient);

	return true;

}


