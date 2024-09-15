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
#include <lut-calibrator/BoardUtils.h>

byte3  CapturedColor::totalMinYUV, CapturedColor::totalMaxYUV;

void CapturedColor::resetTotalRange()
{
	totalMinYUV = { 255, 255, 255 };
	totalMaxYUV = { 0,0,0 };
}

bool CapturedColor::calculateFinalColor()
{
	if (count == 0 || (linalg::maxelem(max - min) > BoardUtils::SCREEN_MAX_COLOR_NOISE_ERROR))
		return false;

	totalMinYUV = { std::min(totalMinYUV.x, min.x), std::min(totalMinYUV.y, min.y), std::min(totalMinYUV.z, min.z) };
	totalMaxYUV = { std::max(totalMaxYUV.x, max.x), std::max(totalMaxYUV.y, max.y), std::max(totalMaxYUV.z, max.z) };

	colorInt = ColorSpaceMath::to_byte3(color / count);
	if (sourceRGB.x == sourceRGB.y && sourceRGB.y == sourceRGB.z)
	{
		colorInt.y = colorInt.z = 128;
		color.y = color.z = (count * 128.0);
	}
	color /= (count * 255.0);
	ColorSpaceMath::trim01(color);

	return true;
}

void CapturedColor::addColor(ColorRgb i)
{
	color += double3(i.red, i.green, i.blue);

	if (count == 0 || min.x > i.red)
		min.x = i.red;
	if (count == 0 || min.y > i.green)
		min.y = i.green;
	if (count == 0 || min.z > i.blue)
		min.z = i.blue;

	if (count == 0 || max.x < i.red)
		max.x = i.red;
	if (count == 0 || max.y < i.green)
		max.y = i.green;
	if (count == 0 || max.z < i.blue)
		max.z = i.blue;

	count++;
}

void CapturedColor::addColor(double3 i)
{
	color += i;

	if (count == 0 || min.x > i.x)
		min.x = i.x;
	if (count == 0 || min.y > i.y)
		min.y = i.y;
	if (count == 0 || min.z > i.z)
		min.z = i.z;

	if (count == 0 || max.x < i.x)
		max.x = i.x;
	if (count == 0 || max.y < i.y)
		max.y = i.y;
	if (count == 0 || max.z < i.z)
		max.z = i.z;

	count++;
}

byte3 CapturedColor::getMaxYUV()
{
	return totalMaxYUV;
}

byte3 CapturedColor::getMinYUV()
{
	return totalMinYUV;
}


QString CapturedColor::toString()
{
	return QString("(%1, %2, %3)").arg(color.x).arg(color.y).arg(color.z);
}

void  CapturedColor::setSourceRGB(byte3 _color)
{
	sourceRGB = ColorSpaceMath::to_int3(_color);
	sourceRGBdelta = linalg::dot(sourceRGB, sourceRGB);
}

int3 CapturedColor::getSourceRGB() const
{
	return sourceRGB;
}


void  CapturedColor::setFinalRGB(double3 _color)
{
	finalRGB = ColorSpaceMath::to_byte3(_color * 255.0);
}

byte3 CapturedColor::getFinalRGB() const
{
	return finalRGB;
}

int CapturedColor::getSourceError(const int3& _color)
{	
	if (sourceRGBdelta == 0)
	{
		return linalg::dot(_color, _color) * 100;
	}

	auto delta = linalg::abs( sourceRGB - _color);
	
	return (delta.x* delta.x* delta.x + delta.y * delta.y * delta.y + delta.z * delta.z * delta.z);
}
