/* Animation_Police.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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

#include <effectengine/Animation_Police.h>

Animation_Police::Animation_Police(QString name) :
	AnimationBase(name)
{
	rotationTime = 2;
	colorOne = { 255, 0, 0 };
	colorTwo = { 0, 0, 255 };
	reverse = false;
	ledData.resize(0);
	colorsCount = 1000;
	increment = 1;
};



void Animation_Police::Init(
	QImage& hyperImage,
	int hyperLatchTime
)
{

}

bool Animation_Police::Play(QPainter* painter)
{
	return true;
}

bool Animation_Police::hasLedData(QVector<ColorRgb>& buffer)
{
	if (buffer.length() != ledData.length() && buffer.length() > 1)
	{
		int hledCount = buffer.length();
		ledData.clear();

		rotationTime = std::max(0.1, rotationTime);
		colorsCount = std::min(hledCount / 2, colorsCount);

		Point3d    hsv1, hsv2;
		uint16_t   hsv1x, hsv2x;

		ColorSys::rgb2hsv(colorOne.x, colorOne.y, colorOne.z, hsv1x, hsv1.y, hsv1.z);
		ColorSys::rgb2hsv(colorTwo.x, colorTwo.y, colorTwo.z, hsv2x, hsv2.y, hsv2.z);

		for (int i = 0; i < hledCount; i++)
		{
			ColorRgb newColor{ 0,0,0 };
			if (i <= colorsCount)
				ColorSys::hsv2rgb(hsv1x, hsv1.y, hsv1.z, newColor.red, newColor.green, newColor.blue);
			else if ((i >= hledCount / 2 - 1) && (i < (hledCount / 2) + colorsCount))
				ColorSys::hsv2rgb(hsv2x, hsv2.y, hsv2.z, newColor.red, newColor.green, newColor.blue);
			ledData.append(newColor);
		}

		increment = 2;
		double sleepTime = rotationTime / hledCount;
		while (sleepTime < 0.05)
		{
			increment *= 2;
			sleepTime *= 2;
		}
		SetSleepTime(int(round(sleepTime * 1000.0)));

		increment %= hledCount;
		ledData.swap(buffer);
	}

	if (buffer.length() == ledData.length())
	{
		if (reverse)
		{
			for (int i = 0; i < increment; i++)
				buffer.move(0, buffer.size() - 1);
		}
		else
		{
			for (int i = 0; i < increment; i++)
				buffer.move(buffer.size() - 1, 0);
		}

	}
	return true;
}


