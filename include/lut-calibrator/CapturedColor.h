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


struct CapturedColor
{
	ColorRgb sourceRGB;
	double3	color, min, max;
	vec<uint8_t, 3> colorInt;
	int count = 0;

	CapturedColor() = default;

	const double& y() const { return color.x; }
	const double& u() const { return color.y; }
	const double& v() const { return color.z; }
	const double3& yuv() const { return color; }

	const uint8_t& Y() const { return colorInt.x; }
	const uint8_t& U() const { return colorInt.y; }
	const uint8_t& V() const { return colorInt.z; }

	bool calculateFinalColor();
	void addColor(ColorRgb i);
	void setSourceRGB(ColorRgb _color);
	ColorRgb getSourceRGB() const;

	QString toString();
};
