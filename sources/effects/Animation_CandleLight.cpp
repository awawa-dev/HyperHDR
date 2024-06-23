/* Animation_CandleLight.cpp
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

#include <effects/Animation_CandleLight.h>

// Candleflicker effect by penfold42
// Algorithm courtesy of
// https://cpldcpu.com/2013/12/08/hacking-a-candleflicker-led/

Animation_CandleLight::Animation_CandleLight() :
	AnimationBase()
{
	colorShift = 4;
	color = { 255, 138, 0 };
	hsv = { 0, 0, 0 };
};

void Animation_CandleLight::Init(
	HyperImage& hyperImage,
	int hyperLatchTime
)
{
	ColorRgb::rgb2hsv(color.x, color.y, color.z, hsv.x, hsv.y, hsv.z);
	SetSleepTime((int)(0.20 * 1000.0));
}

bool Animation_CandleLight::Play(HyperImage& painter)
{
	return true;
}

Point3d Animation_CandleLight::CandleRgb()
{
	Point3d frgb;
	int		RAND = (rand() % (colorShift * 2 + 1)) - colorShift;
	int		hue = (hsv.x + RAND + 360) % 360;
	int		breakStop = 0;

	RAND = rand() % 16;
	while ((RAND & 0x0c) == 0 && (breakStop++ < 100)) {
		RAND = rand() % 16;
	}

	int val = std::min(RAND * 17, 255);

	ColorRgb::hsv2rgb(hue, hsv.y, val, frgb.x, frgb.y, frgb.z);

	return frgb;
}

bool Animation_CandleLight::hasLedData(std::vector<ColorRgb>& buffer)
{
	if (buffer.size() > 1)
	{
		ledData.clear();

		for (size_t i = 0; i < buffer.size(); i++)
		{
			Point3d  color = CandleRgb();
			ColorRgb newColor{ color.x, color.y, color.z };

			ledData.push_back(newColor);
		}

		ledData.swap(buffer);
	}

	return true;
}


