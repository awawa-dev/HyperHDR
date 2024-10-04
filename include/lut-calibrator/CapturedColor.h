#pragma once

/* CapturedColor.h
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

#ifndef PCH_ENABLED
	#include <QString>
	#include <cmath>
	#include <cstring>
	#include <vector>
#endif

#include <linalg.h>
#include <lut-calibrator/ColorSpace.h>
#include <image/ColorRgb.h>
#include <image/Image.h>


class CapturedColor
{
private:
	int totalSamples = 0;
	int3 sourceRGB;
	int sourceRGBdelta = 0;
	std::list<byte3> finalRGB;
	double3	color;
	std::list<std::pair<byte3, int>> inputColors;
	std::list<std::pair<byte3, int>> sortedInputYUVColors;
	std::list<std::pair<double3, int>> sortedInputYuvColors;
	byte3 min, max;
	byte3 colorInt;

public:
	CapturedColor() = default;

	const double& y() const { return color.x; }
	const double3& yuv() const { return color; }

	const uint8_t& Y() const { return colorInt.x; }
	const uint8_t& U() const { return colorInt.y; }
	const uint8_t& V() const { return colorInt.z; }

	void importColors(const CapturedColor& color);
	bool calculateFinalColor();
	bool hasAllSamples();
	bool hasAnySample();
	std::list<std::pair<byte3, int>> getInputYUVColors() const;
	std::list<std::pair<double3, int>> getInputYuvColors() const;
	void addColor(ColorRgb i);
	void addColor(const byte3& i);
	void setSourceRGB(byte3 _color);
	int getSourceError(const int3& _color) const;
	int3 getSourceRGB() const;
	void setFinalRGB(byte3 input);
	std::list<byte3> getFinalRGB() const;

	QString toString();
};
