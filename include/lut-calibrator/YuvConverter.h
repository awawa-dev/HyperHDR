#pragma once

/* YuvConverter.h
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
#endif

#include <linalg.h>
#include <lut-calibrator/ColorSpace.h>

using namespace linalg;
using namespace aliases;

class YuvConverter
{
public:
	enum YUV_COEFS { FCC = 0, BT601 = 1, BT709 = 2, BT2020 = 3 };
	enum COLOR_RANGE { UNKNOWN = 0, FULL = 1, LIMITED = 2 };
	enum YUV_DIRECTION { FROM_RGB_TO_YUV = 0, FROM_YUV_TO_RGB = 1 };

	const std::map<YUV_COEFS, double2> knownCoeffs = {
				{YUV_COEFS::FCC,      {0.3,    0.11  } },
				{YUV_COEFS::BT601,    {0.2990, 0.1140} },
				{YUV_COEFS::BT709,    {0.2126, 0.0722} },
				{YUV_COEFS::BT2020,   {0.2627, 0.0593} }
	};

	double3 multiplyColorMatrix(double4x4 matrix, const double3& input) const;
	double3 toRgb(COLOR_RANGE range, YUV_COEFS coef, const double3& input) const;
	double3 toYuv(COLOR_RANGE range, YUV_COEFS coef, const double3& input) const;
	double3 toYuvBT709(COLOR_RANGE range, const double3& input) const;
	QString coefToString(YUV_COEFS cf) const;
	YuvConverter();
	QString toString();
	double2 getCoef(YUV_COEFS cf);
	byte3 yuv_to_rgb(YUV_COEFS coef, COLOR_RANGE range, const byte3& input) const;
	double4x4 create_yuv_to_rgb_matrix(COLOR_RANGE range, double Kr, double Kb) const;

private:
	std::map<COLOR_RANGE, std::map<YUV_COEFS, double4x4>> yuv2rgb;
	std::map<COLOR_RANGE, std::map<YUV_COEFS, double4x4>> rgb2yuv;
	std::map<COLOR_RANGE, double4x4> rgb2yuvBT709;
};
