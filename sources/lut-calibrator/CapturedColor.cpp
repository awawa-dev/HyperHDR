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

	while(sortedInputYUVColors.size() > 3 || (sortedInputYUVColors.size() == 3 && sortedInputYUVColors.back().second < 6))
		sortedInputYUVColors.pop_back();
	
	std::for_each(sortedInputYUVColors.begin(), sortedInputYUVColors.end(), [this](std::pair<byte3, int>& m) {

		/*if (m.first.y >= 127 && m.first.y <= 129 && m.first.z >= 127 && m.first.z <= 129)
		{
			m.first.y = 128;
			m.first.z = 128;
		}*/

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


void CapturedColor::setCoords(const byte3& index)
{
	arrayCoords = index;

	int MAX_IND = BoardUtils::MAX_INDEX;

	if ((std::min(index.x, index.z) == 0 && std::max(index.x, index.z) == MAX_IND && (index.y % (MAX_IND / 2) == 0)) ||
		(std::min(index.x, index.y) == 0 && std::max(index.x, index.y) == MAX_IND && (index.z % (MAX_IND / 2) == 0)) ||
		(std::min(index.y, index.z) == 0 && std::max(index.y, index.z) == MAX_IND && (index.x % (MAX_IND / 2) == 0)) ||
		(index.x == MAX_IND && index.y == MAX_IND && index.z == MAX_IND / 2) ||
		(index.x == MAX_IND && index.y == MAX_IND / 2 && index.z == MAX_IND)
		)
	{
		lchPrimary = LchPrimaries::HIGH;
	}
	else
	{
		MAX_IND /= 2;
		if (std::max(std::max(index.x, index.y), index.z) == MAX_IND &&
			((std::min(index.x, index.z) == 0 && std::max(index.x, index.z) == MAX_IND && (index.y % (MAX_IND / 2) == 0)) ||
			(std::min(index.x, index.y) == 0 && std::max(index.x, index.y) == MAX_IND && (index.z % (MAX_IND / 2) == 0)) ||
			(std::min(index.y, index.z) == 0 && std::max(index.y, index.z) == MAX_IND && (index.x % (MAX_IND / 2) == 0)) ||
			(index.x == MAX_IND && index.y == MAX_IND && index.z == MAX_IND / 2) ||
			(index.x == MAX_IND && index.y == MAX_IND / 2 && index.z == MAX_IND)
			))
		{
			lchPrimary = LchPrimaries::LOW;
		}
		else
		{
			lchPrimary = LchPrimaries::NONE;
		}
	}
}

QString CapturedColor::toString()
{
	return QString("(%1, %2, %3)").arg(color.x).arg(color.y).arg(color.z);
}

void  CapturedColor::setSourceRGB(byte3 _color)
{
	sourceRGB = ColorSpaceMath::to_int3(_color);

	auto srgb = static_cast<double3>(sourceRGB) / 255.0;
	sourceLch = ColorSpaceMath::xyz_to_lch(ColorSpaceMath::from_sRGB_to_XYZ(srgb) * 100.0);
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

CapturedColor::LchPrimaries CapturedColor::isLchPrimary(double3* _lchCoords) const
{
	if (_lchCoords != nullptr)
	{
		*_lchCoords = sourceLch;
	}
	return lchPrimary;
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
	constexpr int LIMIT_MAX_UP = 248;
	constexpr int LIMIT_MAX_DOWN = 232;
	constexpr int LIMIT_MIDDLE_UP = 228;
	constexpr int LIMIT_MIDDLE_DOWN = 220;

	if ((arrayCoords.x == BoardUtils::MAX_INDEX - 1 && arrayCoords.x == arrayCoords.y && arrayCoords.y == arrayCoords.z &&
		(_color.x > LIMIT_MAX_UP || _color.x < LIMIT_MAX_DOWN)))
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if ((arrayCoords.x == BoardUtils::MAX_INDEX - 2 && arrayCoords.x == arrayCoords.y && arrayCoords.y == arrayCoords.z &&
		(_color.x > LIMIT_MIDDLE_UP || _color.x < LIMIT_MIDDLE_DOWN)))
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if (arrayCoords.x == BoardUtils::MAX_INDEX && arrayCoords.y == 0 && arrayCoords.z == 0 &&
		_color.x < LIMIT_MIDDLE_DOWN)
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if (arrayCoords.y == BoardUtils::MAX_INDEX && arrayCoords.x == 0 && arrayCoords.z == 0 &&
		_color.y < LIMIT_MIDDLE_DOWN)
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if (arrayCoords.z == BoardUtils::MAX_INDEX && arrayCoords.y == 0 && arrayCoords.x == 0 &&
		_color.z < LIMIT_MIDDLE_DOWN)
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if ((arrayCoords.x == BoardUtils::MAX_INDEX - 2 && arrayCoords.x == arrayCoords.y && arrayCoords.z == 0 &&
		((_color.x + _color.y) > LIMIT_MIDDLE_UP * 2)))
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if ((arrayCoords.x == BoardUtils::MAX_INDEX - 2 && arrayCoords.x == arrayCoords.z && arrayCoords.y == 0 &&
		((_color.x + _color.z) > LIMIT_MIDDLE_UP * 2)))
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if ((arrayCoords.y == BoardUtils::MAX_INDEX - 2 && arrayCoords.y == arrayCoords.z && arrayCoords.x == 0 &&
		((_color.y + _color.z) > LIMIT_MIDDLE_UP * 2)))
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if ((arrayCoords.x == BoardUtils::MAX_INDEX - 1 && arrayCoords.x == arrayCoords.y && arrayCoords.z == 0 &&
		((_color.x + _color.y) > LIMIT_MAX_UP * 2)))
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if ((arrayCoords.x == BoardUtils::MAX_INDEX - 1 && arrayCoords.x == arrayCoords.z && arrayCoords.y == 0 &&
		((_color.x + _color.z) > LIMIT_MAX_UP * 2)))
		return BoardUtils::MAX_CALIBRATION_ERROR;

	if ((arrayCoords.y == BoardUtils::MAX_INDEX - 1 && arrayCoords.y == arrayCoords.z && arrayCoords.x == 0 &&
		((_color.y + _color.z) > LIMIT_MAX_UP * 2)))
		return BoardUtils::MAX_CALIBRATION_ERROR;

	auto delta = linalg::abs( sourceRGB - _color);


	if (sourceRGB.x == sourceRGB.y && sourceRGB.y == sourceRGB.z)
	{
		return (delta.x * delta.x * delta.x + delta.y * delta.y * delta.y + delta.z * delta.z * delta.z) * 100;
	}
	else if (sourceRGB.x == sourceRGB.y)
	{
		auto diff = std::abs(_color.x - _color.y);
		if (_color.x > _color.y)
		{
			delta.x = std::min(delta.x, diff);
			delta.y = std::min(delta.y, diff);
		}
		else
		{
			delta.x = std::max(delta.x, diff);
			delta.y = std::max(delta.y, diff);
		}
	}
	else if (sourceRGB.x == sourceRGB.z)
	{
		auto diff = std::abs(_color.x - _color.z);

		if (_color.z > _color.x)
		{
			delta.x = std::min(delta.x, diff);
			delta.z = std::min(delta.z, diff);
		}
		else
		{
			delta.x = std::max(delta.x, diff);
			delta.z = std::max(delta.z, diff);
		}
	}
	else if (sourceRGB.y == sourceRGB.z)
	{
		auto diff = std::abs(_color.y - _color.z);

		if (_color.z > _color.y)
		{
			delta.y = std::min(delta.y, diff);
			delta.z = std::min(delta.z, diff);
		}
		else
		{
			delta.y = std::max(delta.y, diff);
			delta.z = std::max(delta.z, diff);
		}
	}
	

	return (delta.x* delta.x* delta.x + delta.y * delta.y * delta.y + delta.z * delta.z * delta.z);
}
