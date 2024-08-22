/* CapturedColor.cpp
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


#include <lut-calibrator/CapturedColor.h>

void CapturedColor::trim01(double3& input)
{
	for (int i = 0; i < 3; i++)
	{
		if (input[i] > 1.0)
			input[i] = 1.0;
		else if (input[i] < 0.0)
			input[i] = 0.0;
	}
}

bool CapturedColor::calculateFinalColor()
{
	const double maxNoice = 10;
	if (count == 0 || (linalg::maxelem(max - min) > maxNoice))
		return false;

	auto tempColor = color / count;
	colorInt = vec<uint8_t, 3>(std::round(tempColor.x), std::round(tempColor.y), std::round(tempColor.z));
	color /= (count * 255.0);
	trim01(color);

	return true;
}

void CapturedColor::addColor(ColorRgb i)
{
	color += double3(i.red, i.green, i.blue);

	if (count == 0 || color.x > i.red)
		min.x = i.red;
	if (count == 0 || color.y > i.green)
		min.y = i.green;
	if (count == 0 || color.z > i.blue)
		min.z = i.blue;

	if (count == 0 || color.x < i.red)
		max.x = i.red;
	if (count == 0 || color.y < i.green)
		max.y = i.green;
	if (count == 0 || color.z < i.blue)
		max.z = i.blue;

	count++;
}


QString CapturedColor::toString()
{
	return QString("(%1, %2, %3)").arg(color.x).arg(color.y).arg(color.z);
}

void  CapturedColor::setSourceRGB(ColorRgb _color)
{
	sourceRGB = _color;
}

ColorRgb CapturedColor::getSourceRGB() const
{
	return sourceRGB;
}
