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


void CapturedColor::importColors(const CapturedColor& color)
{
	for (auto iter = color.inputColors.begin(); iter != color.inputColors.end(); ++iter)
	{
		for (int i = 0; i < (*iter).second; i++)
		{
			addColor((*iter).first);
		}
	}
	calculateFinalColor();
}

bool CapturedColor::calculateFinalColor()
{
	if (inputColors.empty() || (linalg::maxelem(max - min) > BoardUtils::SCREEN_MAX_COLOR_NOISE_ERROR))
		return false;

	int count = 0;
	color = double3{ 0,0,0 };

	sortedInputYUVColors.clear();
	sortedInputYuvColors.clear();
	for (auto iter = inputColors.begin(); iter != inputColors.end(); ++iter)
	{
		color += ((*iter).first) * ((*iter).second);
		count += ((*iter).second);

		// sort
		bool inserted = false;
		for (auto sorted = sortedInputYUVColors.begin(); sorted != sortedInputYUVColors.end(); ++sorted)
			if (((*iter).second) > (*sorted).second)
			{
				sortedInputYUVColors.insert(sorted, std::pair<byte3, int>((*iter).first, (*iter).second));
				inserted = true;
				break;
			}

		if (!inserted)
			sortedInputYUVColors.push_back(std::pair<byte3, int>((*iter).first, (*iter).second));
	}

	while(sortedInputYUVColors.size() > 3 || (sortedInputYUVColors.size() == 3 && sortedInputYUVColors.back().second <= 6))
		sortedInputYUVColors.pop_back();
	
	std::for_each(sortedInputYUVColors.begin(), sortedInputYUVColors.end(), [this](std::pair<byte3, int>& m) {

		if (m.first.y >= 127 && m.first.y <= 129 && m.first.z >= 127 && m.first.z <= 129)
		{
			m.first.y = 128;
			m.first.z = 128;
		}

		sortedInputYuvColors.push_back(std::pair<double3, int>(static_cast<double3>(m.first) / 255.0, m.second));
	});
	

	auto workColor = color / count;

	colorInt = ColorSpaceMath::to_byte3(workColor);
	color /= (count * 255.0);
	
	if (sourceRGB.x == sourceRGB.y && sourceRGB.y == sourceRGB.z)
	{
		colorInt.y = colorInt.z = 128;
		color.y = color.z = 128.0/255;
	}
	else
	{
		auto delta = workColor - colorInt;

		if (delta.y * delta.z < 0)
		{
			if (((fabs(delta.y) >= fabs(delta.z)) && delta.y > 0) ||
				((fabs(delta.y) < fabs(delta.z)) && delta.z > 0))
			{
				colorInt.y = std::floor(workColor.y);
				colorInt.z = std::floor(workColor.z);
			}
			else if (((fabs(delta.y) >= fabs(delta.z)) && delta.y < 0) ||
				((fabs(delta.y) < fabs(delta.z)) && delta.z < 0))
			{
				colorInt.y = std::ceil(workColor.y);
				colorInt.z = std::ceil(workColor.z);
			}
		}
	}
	
	ColorSpaceMath::trim01(color);

	return true;
}

bool CapturedColor::hasAllSamples()
{
	return totalSamples > 27;
}

bool CapturedColor::hasAnySample()
{
	return totalSamples > 0;
}


void CapturedColor::addColor(ColorRgb i)
{
	addColor(byte3{ i.red, i.green, i.blue });
}

void CapturedColor::addColor(const byte3& i)
{
	bool empty = !hasAnySample();

	if (empty || min.x > i.x)
		min.x = i.x;
	if (empty || min.y > i.y)
		min.y = i.y;
	if (empty || min.z > i.z)
		min.z = i.z;

	if (empty || max.x < i.x)
		max.x = i.x;
	if (empty || max.y < i.y)
		max.y = i.y;
	if (empty || max.z < i.z)
		max.z = i.z;
	
	auto findIter = std::find_if(inputColors.begin(), inputColors.end(), [&](auto& m) {
		return m.first == i;
	});

	if (findIter == inputColors.end())
	{
		inputColors.push_back(std::pair<byte3, int>(i, 1));
	}
	else
	{
		(*findIter).second++;
	}
	

	totalSamples++;
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


void  CapturedColor::setFinalRGB(byte3 input)
{
	bool found = (std::find(finalRGB.begin(), finalRGB.end(), input) != finalRGB.end());
	if (!found)
	{
		finalRGB.push_back(input);
	}
}

std::list<byte3> CapturedColor::getFinalRGB() const
{
	return finalRGB;
}

std::list<std::pair<byte3, int>> CapturedColor::getInputYUVColors() const
{
	return sortedInputYUVColors;
}

std::list<std::pair<double3, int>> CapturedColor::getInputYuvColors() const
{
	return sortedInputYuvColors;
}

int CapturedColor::getSourceError(const int3& _color) const
{	
	if (sourceRGBdelta == 0)
	{
		return linalg::dot(_color, _color) * 100;
	}

	auto delta = linalg::abs( sourceRGB - _color);
	
	return (delta.x* delta.x* delta.x + delta.y * delta.y * delta.y + delta.z * delta.z * delta.z);
}
