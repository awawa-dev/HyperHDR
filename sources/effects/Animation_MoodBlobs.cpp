/* Animation_MoodBlobs.cpp
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

#include <effects/Animation_MoodBlobs.h>

#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

Animation_MoodBlobs::Animation_MoodBlobs() :
	AnimationBase()
{

	fullColorWheelAvailable = false;
	baseColorChangeIncreaseValue = 0;
	sleepTime = 1;
	amplitudePhaseIncrement = 0;
	colorDataIncrement = 1;
	amplitudePhase = 0;
	rotateColors = false;
	baseColorChangeStepCount = 0;
	baseHSVValue = 0;
	baseHsv = { 0,0,0 };
	numberOfRotates = 0;



	rotationTime = 20.0;
	color = { 0,0,255 };
	colorRandom = false;
	hueChange = 60.0;
	blobs = 5;
	reverse = false;
	baseColorChange = false;
	baseColorRangeLeft = 0.0;
	baseColorRangeRight = 360.0;
	baseColorChangeRate = 10.0;
	hyperledCount = -1;
};



void Animation_MoodBlobs::Init(
	HyperImage& hyperImage,
	int hyperLatchTime
)
{

}

bool Animation_MoodBlobs::Play(HyperImage& painter)
{
	return true;
}

bool Animation_MoodBlobs::hasLedData(std::vector<ColorRgb>& buffer)
{
	if (static_cast<int>(buffer.size()) != hyperledCount && buffer.size() > 1)
	{
		hyperledCount = buffer.size();


		if ((baseColorRangeRight > baseColorRangeLeft && (baseColorRangeRight - baseColorRangeLeft) < 10) ||
			(baseColorRangeLeft > baseColorRangeRight && ((baseColorRangeRight + 360) - baseColorRangeLeft) < 10))
			baseColorChange = false;


		fullColorWheelAvailable = (fmod(baseColorRangeRight, 360)) == (fmod(baseColorRangeLeft, 360));
		baseColorChangeIncreaseValue = 1.0 / 360.0;
		hueChange /= 360.0;
		baseColorRangeLeft = (baseColorRangeLeft / 360.0);
		baseColorRangeRight = (baseColorRangeRight / 360.0);


		rotationTime = std::max(0.1, rotationTime);
		hueChange = std::max(0.0, std::min(std::fabs(hueChange), .5));
		blobs = std::max(1, blobs);
		baseColorChangeRate = std::max(0.0, baseColorChangeRate);

		ColorRgb::rgb2hsv(color.x, color.y, color.z, baseHsv.x, baseHsv.y, baseHsv.z);

		double baseHsvx = baseHsv.x / double(360);
		if (colorRandom)
		{
			baseHsvx = int((360.0 * static_cast <float> (rand())) / static_cast <float> (RAND_MAX));
		}

		colorData.clear();
		for (int i = 0; i < hyperledCount; i++)
		{
			Point3d rgb;
			int hue = (int(360.0 * fmod((baseHsvx + hueChange * std::sin(2 * M_PI * i / hyperledCount)), 1.0)));
			while (hue < 0)
				hue += 360;
			ColorRgb::hsv2rgb(hue, baseHsv.y, baseHsv.z, rgb.x, rgb.y, rgb.z);
			colorData.push_back(rgb);
		}


		sleepTime = 0.1;
		amplitudePhaseIncrement = blobs * M_PI * sleepTime / rotationTime;
		colorDataIncrement = 1;
		baseColorChangeRate /= sleepTime;

		if (reverse)
		{
			amplitudePhaseIncrement = -amplitudePhaseIncrement;
			colorDataIncrement = -colorDataIncrement;
		}

		amplitudePhase = 0.0;
		rotateColors = false;
		baseColorChangeStepCount = 0;
		baseHSVValue = baseHsvx;
		numberOfRotates = 0;


		SetSleepTime(int(round(sleepTime * 1000.0)));
	}

	if (static_cast<int>(buffer.size()) == hyperledCount)
	{
		if (baseColorChange)
		{
			if (baseColorChangeStepCount >= baseColorChangeRate)
			{
				baseColorChangeStepCount = 0;

				if (fullColorWheelAvailable)
					baseHSVValue = std::fmod((baseHSVValue + baseColorChangeIncreaseValue), baseColorRangeRight);
				else
				{
					if (baseColorChangeIncreaseValue < 0 && baseHSVValue > baseColorRangeLeft && (baseHSVValue + baseColorChangeIncreaseValue) <= baseColorRangeLeft)
						baseColorChangeIncreaseValue = abs(baseColorChangeIncreaseValue);
					else if (baseColorChangeIncreaseValue > 0 && baseHSVValue < baseColorRangeRight && (baseHSVValue + baseColorChangeIncreaseValue) >= baseColorRangeRight)
						baseColorChangeIncreaseValue = -abs(baseColorChangeIncreaseValue);

					baseHSVValue = fmod((baseHSVValue + baseColorChangeIncreaseValue), 1.0);
				}

				colorData.clear();
				for (int i = 0; i < hyperledCount; i++)
				{
					Point3d rgb;
					int hue = (int(360.0 * fmod((baseHSVValue + hueChange * std::sin(2 * M_PI * i / hyperledCount)), 1.0)));
					while (hue < 0)
						hue += 360;
					ColorRgb::hsv2rgb(hue, baseHsv.y, baseHsv.z, rgb.x, rgb.y, rgb.z);
					colorData.push_back(rgb);
				}

				if (colorDataIncrement >= 0)
					for (int i = 0; i < colorDataIncrement * numberOfRotates; i++)
					{
						auto last = colorData.back();
						colorData.pop_back();
						colorData.insert(colorData.begin(), last);
					}
				else
					for (int i = 0; i < -colorDataIncrement * numberOfRotates; i++)
					{
						auto first = colorData.front();
						colorData.erase(colorData.begin());
						colorData.push_back(first);
					}
			}
			baseColorChangeStepCount += 1;
		}

		for (int i = 0; i < hyperledCount; i++)
		{
			double amplitude = std::max(0.0, std::sin(-amplitudePhase + (2 * M_PI * blobs * i) / double(hyperledCount)));
			buffer[i].red = int(colorData[i].x * amplitude);
			buffer[i].green = int(colorData[i].y * amplitude);
			buffer[i].blue = int(colorData[i].z * amplitude);
		}

		amplitudePhase = std::fmod((amplitudePhase + amplitudePhaseIncrement), (2 * M_PI));

		if (rotateColors)
		{
			if (colorDataIncrement >= 0)
				for (int i = 0; i < colorDataIncrement; i++)
				{
					auto last = colorData.back();
					colorData.pop_back();
					colorData.insert(colorData.begin(), last);
				}
			else
				for (int i = 0; i < -colorDataIncrement; i++)
				{
					auto first = colorData.front();
					colorData.erase(colorData.begin());
					colorData.push_back(first);
				}
			numberOfRotates = (numberOfRotates + 1) % hyperledCount;
		}
		rotateColors = !rotateColors;
	}
	return true;
}


