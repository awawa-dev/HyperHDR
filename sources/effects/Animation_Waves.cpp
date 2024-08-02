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
#include <effects/Animation_Waves.h>
#include <algorithm>

Animation_Waves::Animation_Waves() :
	AnimationBase()
{
	rotation_time = 90.0;
	reverse = false;
	center_x = -0.15;
	center_y = -0.25;
	random_center = false;

	pointS1 = Point2d({ 0, 0 });

	SetSleepTime(100);

	custom_colors.push_back({ 255,0,0 });
	custom_colors.push_back({ 255,255,0 });
	custom_colors.push_back({ 0,255,0 });

	custom_colors.push_back({ 0,255,255 });
	custom_colors.push_back({ 0,0,255 });
	custom_colors.push_back({ 255,0,255 });

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


	if (custom_colors.size() > 0)
	{
		int pos = 0;
		int step = int(255 / custom_colors.size());
		for (int i = 0; i < static_cast<int>(custom_colors.size()); i++)
		{
			positions.push_back(pos % 256);
			pos += step;
		}
	}

	reverse_time *= 1000;
	targetTime = InternalClock::now() + reverse_time;
}

bool Animation_Waves::Play(HyperImage& painter)
{
	bool ret = true;

	if (custom_colors.size() == 0)
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


	std::vector<Animation_Swirl::SwirlGradient> gradientBa;
	int it = 0;

	for(const auto& color : custom_colors)
	{
		Animation_Swirl::SwirlGradient elem{ positions[it], color.items[0], color.items[1], color.items[2] };
		gradientBa.push_back(elem);
		it += 1;
	}

	std::sort(gradientBa.begin(), gradientBa.end(),
		[](const Animation_Swirl::SwirlGradient& a, const Animation_Swirl::SwirlGradient& b) -> bool {
			return a.items[0] < b.items[0];
	});

	if (gradientBa.size() > 1)
	{
		int l = gradientBa.size() - 1;
		if (gradientBa[0].items[0] != 0 || gradientBa[l].items[0] != 255)
		{
			
			float asp2 = gradientBa[0].items[0] / ((float)gradientBa[1].items[0] - (float)gradientBa[0].items[0]);
			float asp1 = 1.0 - asp2;

			Animation_Swirl::SwirlGradient elem{ 0,
				ColorRgb::clamp(gradientBa[0].items[1] * asp1 + gradientBa[l].items[1] * asp2),
				ColorRgb::clamp(gradientBa[0].items[2] * asp1 + gradientBa[l].items[2] * asp2),
				ColorRgb::clamp(gradientBa[0].items[3] * asp1 + gradientBa[l].items[3] * asp2) };
			gradientBa.insert(gradientBa.begin(), elem);
			Animation_Swirl::SwirlGradient elem2{ 255,
				ColorRgb::clamp(gradientBa[l].items[1] * asp2 + gradientBa[l].items[1] * asp1),
				ColorRgb::clamp(gradientBa[0].items[2] * asp2 + gradientBa[l].items[2] * asp1),
				ColorRgb::clamp(gradientBa[0].items[3] * asp2 + gradientBa[l].items[3] * asp1) };
			gradientBa.push_back(elem2);

		}
	}

	imageRadialGradient(painter, pointS1.x, pointS1.y, diag, gradientBa);

	for (int i = 0; i < static_cast<int>(positions.size()); i++)
	{
		int pos = positions[i];
		if (reverse)
			positions[i] = (pos >= 1) ? pos - 1 : 255;
		else
			positions[i] = (pos <= 254) ? pos + 1 : 0;
	}

	return ret;
}


bool Animation_Waves::imageRadialGradient(HyperImage& painter, int centerX, int centerY, int angle, const std::vector<Animation_Swirl::SwirlGradient>& bytearray)
{

	angle = std::max(std::min(angle, 360), 0);

	std::vector<uint8_t> arr;
	for(const Animation_Swirl::SwirlGradient& item : bytearray)
	{
		arr.push_back(item.items[0]);
		arr.push_back(item.items[1]);
		arr.push_back(item.items[2]);
		arr.push_back(item.items[3]);
		arr.push_back(255);
	}
	painter.radialFill(centerX, centerY, angle, arr);

	return true;

}


