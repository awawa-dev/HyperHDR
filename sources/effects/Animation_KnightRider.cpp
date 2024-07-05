/* Animation_KnightRider.cpp
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

#include <effects/Animation_KnightRider.h>

Animation_KnightRider::Animation_KnightRider() :
	AnimationBase()
{
	speed = 0;
	fadeFactor = 0;
	color = { 0, 0, 0 };
	increment = 0;
	position = 0;
	direction = 0;

	for (int j = 0; j < KNIGHT_WIDTH; j++) {
		imageData[3 * j] = 0;
		imageData[3 * j + 1] = 0;
		imageData[3 * j + 2] = 0;
	}
};

EffectDefinition Animation_KnightRider::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_KnightRider>);
	ed.name = ANIM_KNIGHT_RIDER;
	return ed;
}

void Animation_KnightRider::Init(
	HyperImage& hyperImage,
	int hyperLatchTime)
{
	hyperImage.resize(KNIGHT_WIDTH, KNIGHT_HEIGHT);

	speed = 1.0;
	fadeFactor = 0.7;
	color = { 255, 0, 0 };

	imageData[0] = color.x;
	imageData[1] = color.y;
	imageData[2] = color.z;

	increment = 1;
	double sleepTime = 1.0 / (speed * KNIGHT_WIDTH);
	while (sleepTime < 0.05)
	{
		increment *= 2;
		sleepTime *= 2;
	}

	SetSleepTime(int(round(sleepTime * 1000.0)));

	position = 0;
	direction = 1;
}

bool Animation_KnightRider::Play(HyperImage& painter)
{
	bool ret = true;

	for (int i = 0; i < increment; i++)
	{
		position += direction;
		if (position == -1)
		{
			position = 1;
			direction = 1;
		}
		else if (position == KNIGHT_WIDTH)
		{
			position = KNIGHT_WIDTH - 2;
			direction = -1;
		}

		for (int j = 0; j < KNIGHT_WIDTH; j++) {
			imageData[3 * j] = uint8_t(clamp(int(fadeFactor * imageData[3 * j]), 0, 255));
			imageData[3 * j + 1] = uint8_t(clamp(int(fadeFactor * imageData[3 * j + 1]), 0, 255));
			imageData[3 * j + 2] = uint8_t(clamp(int(fadeFactor * imageData[3 * j + 2]), 0, 255));

			painter.setPen(ColorRgb(imageData[3 * j], imageData[3 * j + 1], imageData[3 * j + 2]));
			painter.drawVerticalLine(j, 0, KNIGHT_HEIGHT - 1);


		}

		imageData[3 * position] = color.x;
		imageData[3 * position + 1] = color.y;
		imageData[3 * position + 2] = color.z;

		//QImage img(imageData, KNIGHT_WIDTH, KNIGHT_HEIGHT, KNIGHT_WIDTH, QImage::Format_RGB888);
		painter.setPen(ColorRgb(color.x, color.y, color.z));
		painter.drawVerticalLine(position, 0, KNIGHT_HEIGHT - 1);
	}
	return ret;
}

bool Animation_KnightRider::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_KnightRider::getDefinition());
