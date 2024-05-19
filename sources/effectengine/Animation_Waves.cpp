/* Animation_Waves.cpp
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

#include <utils/InternalClock.h>
#include <effectengine/Animation_Waves.h>

Animation_Waves::Animation_Waves(QString name) :
	AnimationBase(name)
{
	rotation_time = 90.0;
	reverse = false;
	center_x = -0.15;
	center_y = -0.25;
	random_center = false;

	pointS1 = Point2d({ 0, 0 });

	SetSleepTime(100);

	custom_colors.append({ 255,0,0 });
	custom_colors.append({ 255,255,0 });
	custom_colors.append({ 0,255,0 });

	custom_colors.append({ 0,255,255 });
	custom_colors.append({ 0,0,255 });
	custom_colors.append({ 255,0,255 });

	diag = 0;
	positions.clear();
	targetTime = 0;
	reverse_time = 0;
	reverse = false;
};

Point2d Animation_Waves::getPoint(const HyperImage& hyperImage, bool random, double x, double y) {
	Point2d p;

	if (random)
	{
		center_x = x = static_cast <double> (rand()) / static_cast <double> (RAND_MAX);
		center_y = y = static_cast <double> (rand()) / static_cast <double> (RAND_MAX);
	}

	p.x = int(round(x * hyperImage.width()));
	p.y = int(round(y * hyperImage.height()));

	return p;
}

int Animation_Waves::getSTime(int hyperLatchTime, int _rt, double steps = 360)
{
	double sleepTime = std::max(1 / (512 / rotation_time), 0.02);

	return int(round(sleepTime * 1000.0));
}



void Animation_Waves::Init(
	HyperImage& hyperImage,
	int hyperLatchTime
)
{

	hyperImage.resize(80, 45);

	pointS1 = getPoint(hyperImage, random_center, center_x, center_y);

	double cX = center_x, cY = 0.0 + center_y;

	if (center_x < 0.5)
		cX = 1.0 - center_x;
	if (center_y < 0.5)
		cY = 1.0 - center_y;

	rotation_time = std::max(rotation_time, 10.0);
	SetSleepTime(getSTime(hyperLatchTime, rotation_time));

	diag = int(round(std::sqrt((cX * std::pow(hyperImage.width(), 2)) + ((cY * std::pow(hyperImage.height(), 2))))));
	diag = int(diag * 1.3);


	if (custom_colors.length() > 0)
	{
		int pos = 0;
		int step = int(255 / custom_colors.length());
		for (int i = 0; i < custom_colors.length(); i++)
		{
			positions.append(pos % 256);
			pos += step;
		}
	}

	reverse_time *= 1000;
	targetTime = InternalClock::now() + reverse_time;
}

bool Animation_Waves::Play(HyperImage& painter)
{
	bool ret = true;

	if (custom_colors.length() == 0)
		return false;

	if (reverse_time >= 1)
	{
		auto now = InternalClock::now();
		if (now > targetTime)
		{
			reverse = !reverse;
			targetTime = now + (double(reverse_time) * static_cast <double> (rand()) / static_cast <double> (RAND_MAX)) + reverse_time;
		}
	}


	QList<Animation_Swirl::SwirlGradient> gradientBa;
	int it = 0;

	foreach(auto color, custom_colors)
	{
		Animation_Swirl::SwirlGradient elem{ positions[it], color.items[0], color.items[1], color.items[2] };
		gradientBa.append(elem);
		it += 1;
	}

	imageRadialGradient(painter, pointS1.x, pointS1.y, diag, gradientBa);

	for (int i = 0; i < positions.length(); i++)
	{
		int pos = positions[i];
		if (reverse)
			positions[i] = (pos >= 1) ? pos - 1 : 255;
		else
			positions[i] = (pos <= 254) ? pos + 1 : 0;
	}

	return ret;
}


bool Animation_Waves::imageRadialGradient(HyperImage& painter, int centerX, int centerY, int angle, const QList<Animation_Swirl::SwirlGradient>& bytearray)
{
	int startX = 0;
	int startY = 0;
	int width = painter.width();
	int height = painter.height();

	angle = qMax(qMin(angle, 360), 0);


	QRect myQRect(startX, startY, width, height);
	QRadialGradient gradient(QPoint(centerX, centerY), qMax(angle, 0));

	foreach(Animation_Swirl::SwirlGradient item, bytearray)
	{
		gradient.setColorAt(
			((uint8_t)item.items[0]) / 255.0,
			QColor(
				(uint8_t)(item.items[1]),
				(uint8_t)(item.items[2]),
				(uint8_t)(item.items[3])
			));
	}
	gradient.setSpread(static_cast<QGradient::Spread>(0));
	painter->fillRect(myQRect, gradient);

	return true;

}


