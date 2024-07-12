/* Animation_Fade.cpp
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

#include <effects/Animation_Fade.h>
#include <climits>

#define STEP_1   1
#define STEP_2   2
#define STEP_3   3
#define STEP_4   4
#define STEP_5   5
#define STEP_6   6

Animation_Fade::Animation_Fade() :
	AnimationBase()
{
	currentStep = 1;

	fadeInTime = 2000;
	fadeOutTime = 2000;
	colorStart = { 255,174,11 };
	colorEnd = { 0,0,0 };
	colorStartTime = 1000;
	colorEndTime = 1000;
	repeat = 0;
	maintainEndCol = true;
	minStepTime = 0;
	current = { 0,0,0 };

	incrementIn = 0;
	sleepTimeIn = 0;
	incrementOut = 0;
	sleepTimeOut = 0;
	steps = 0;
	repeatCounter = 1;

	indexer6 = 5;
	indexer2 = indexer4 = 0;
	indexer1 = indexer3 = INT_MIN;



};

void Animation_Fade::SetPoint(Point3d point) {
}

#define calcChannel(i) \
	(uint8_t)(std::min(std::max(int(std::round(colorStart.i + color_step.i*step)),0), int((colorStart.i < colorEnd.i) ? colorEnd.i : colorStart.i)))

void Animation_Fade::Init(
	HyperImage& hyperImage,
	int hyperLatchTime
)
{
	fadeInTime /= 1000.0;
	fadeOutTime /= 1000.0;

	colorStartTime /= 1000.0;
	colorEndTime /= 1000.0;

	minStepTime = float(hyperLatchTime) / 1000.0;
	if (minStepTime < 0.012)
		minStepTime = 0.012;


	steps = double(std::max(std::abs(colorEnd.x - colorStart.x),
		std::max(std::abs(colorEnd.y - colorStart.y),
			std::abs(colorEnd.z - colorStart.z))));

	struct { double x, y, z; } color_step = { 0,0,0 };

	if (steps == 0)
		steps = 1;
	else
		color_step = {
			((colorEnd.x - colorStart.x) / steps),
			((colorEnd.y - colorStart.y) / steps),
			((colorEnd.z - colorStart.z) / steps) };



	colors.clear();
	for (int step = 0; step < (int(steps) + 1); step++)
		colors.push_back(Point3d{ calcChannel(x), calcChannel(y), calcChannel(z) });


	if (fadeInTime > 0)
	{
		incrementIn = std::max(1, int(std::round(steps / (fadeInTime / minStepTime))));
		double mstep = fadeInTime / (steps / incrementIn);
		sleepTimeIn = mstep;

		while (sleepTimeIn < 0.015 && mstep > 0.0001)
		{
			sleepTimeIn += mstep;
			incrementIn++;
		}
	}
	else
		incrementIn = sleepTimeIn = 1;

	if (fadeOutTime > 0)
	{
		incrementOut = std::max(1, int(std::round(steps / (fadeOutTime / minStepTime))));
		double mstep = fadeOutTime / (steps / incrementOut);
		sleepTimeOut = mstep;

		while (sleepTimeOut < 0.015 && mstep > 0.0001)
		{
			sleepTimeOut += mstep;
			incrementOut++;
		}
	}
	else
		incrementOut = sleepTimeOut = 1;

	repeatCounter = 1;
}

bool Animation_Fade::Play(HyperImage& painter)
{
	return true;
}

bool Animation_Fade::hasLedData(std::vector<ColorRgb>& buffer)
{
	if (currentStep == STEP_1)
	{
		if (fadeInTime > 0)
		{
			if (indexer1 == INT_MIN && colors.size() > 0)
			{
				indexer1 = 0;
				current = { colors[0].x, colors[0].y, colors[0].z };
				SetSleepTime(sleepTimeIn * 1000);
			}
			else if (indexer1 < int(steps) + 1 && indexer1 < static_cast<int>(colors.size()))
			{
				current = { colors[indexer1].x, colors[indexer1].y, colors[indexer1].z };
				indexer1 += incrementIn;
				SetSleepTime(sleepTimeIn * 1000);
			}
			else
				currentStep = STEP_2;
		}
		else
			currentStep = STEP_2;
	}

	if (currentStep == STEP_2)
	{
		if (indexer2 < colorStartTime && int(steps) < static_cast<int>(colors.size()))
		{
			current = { colors[int(steps)].x,colors[int(steps)].y,colors[int(steps)].z };
			SetSleepTime(minStepTime * 1000);
			indexer2 += minStepTime;
		}
		else
			currentStep = STEP_3;
	}

	if (currentStep == STEP_3)
	{
		if (fadeOutTime > 0)
		{
			if (indexer3 == INT_MIN && int(steps) < static_cast<int>(colors.size()))
			{
				indexer3 = int(steps);
				current = { colors[indexer3].x, colors[indexer3].y, colors[indexer3].z };
				SetSleepTime(sleepTimeOut * 1000);
			}
			else if (indexer3 >= 0 && indexer3 < static_cast<int>(colors.size()))
			{
				current = { colors[indexer3].x, colors[indexer3].y, colors[indexer3].z };
				indexer3 -= incrementOut;
				SetSleepTime(sleepTimeOut * 1000);
			}
			else
				currentStep = STEP_4;
		}
		else
			currentStep = STEP_4;
	}

	if (currentStep == STEP_4)
	{
		if (indexer4 < colorEndTime && colors.size()>0)
		{
			current = { colors[0].x,colors[0].y,colors[0].z };
			SetSleepTime(minStepTime * 1000);
			indexer4 += minStepTime;
		}
		else
			currentStep = STEP_5;
	}

	if (currentStep == STEP_5)
	{
		if (repeat > 0 && repeatCounter >= repeat)
			currentStep = STEP_6;
		else
		{
			repeatCounter += 1;
			indexer2 = indexer4 = 0;
			indexer1 = indexer3 = INT_MIN;
			currentStep = STEP_1;
		}
	}

	if (currentStep == STEP_6)
	{
		if (indexer6-- > 0 || maintainEndCol)
		{
			SetSleepTime(100);
		}
		else
			return false;
	}
	
	std::fill(buffer.begin(), buffer.end(), ColorRgb(current.x, current.y, current.z ));
	return true;
}
