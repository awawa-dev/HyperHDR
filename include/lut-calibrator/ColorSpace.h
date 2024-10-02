#pragma once

/* ColorSpace.h
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
	#include <cmath>
	#include <cfloat>
	#include <climits>
	#include <map>
	#include <QString>
	#include <vector>
#endif

#include <linalg.h>
#include <lut-calibrator/LutCalibrator.h>

using namespace linalg;
using namespace aliases;

namespace ColorSpaceMath
{
	enum PRIMARIES { SRGB = 0, BT_2020, WIDE_GAMMUT };	

	QString gammaToString(HDR_GAMMA gamma);

	constexpr mat<double, 3, 3> matrix(std::array<double, 9> m)
	{
		double3 c1(m[0], m[3], m[6]);
		double3 c2(m[1], m[4], m[7]);
		double3 c3(m[2], m[5], m[8]);
		return double3x3(c1, c2, c3);
	}

	constexpr mat<double, 4, 4> matrix4(std::array<double, 16> m)
	{
		double4 c1(m[0], m[4], m[8], m[12]);
		double4 c2(m[1], m[5], m[9], m[13]);
		double4 c3(m[2], m[6], m[10], m[14]);
		double4 c4(m[3], m[7], m[11], m[15]);
		return double4x4(c1, c2, c3, c4);
	}

	constexpr double3x3 matrix_bt2020_to_XYZ = matrix({
				0.636958,	0.144617,	0.168881,
				0.262700,	0.677998,	0.059302,
				0.000000,	0.028073,	1.060985
		});

	constexpr double3x3 matrix_sRgb_to_XYZ = matrix({
				0.4124564,	0.3575761,	0.1804375,
				0.2126729,	0.7151522,	0.0721750,
				0.0193339,	0.1191920,	0.9503041
		});

	mat<double, 3, 3> getPrimariesToXYZ(PRIMARIES primary);

	double3 bt2020_nonlinear_to_linear(double3 input);

	double3 bt2020_linear_to_nonlinear(double3 input);

	double3 srgb_nonlinear_to_linear(double3 input);
	
	double3 srgb_linear_to_nonlinear(double3 input);

	double3 from_BT2020_to_BT709(double3 a);

	double PQ_ST2084(double scale, double  nonlinear);

	double3 PQ_ST2084(double scale, double3 nonlinear);

	double inverse_OETF_HLG(double input);

	double3 inverse_OETF_HLG(double3 input);

	double3 OOTF_HLG(double3 input, double gamma = 1.2);
	double3 OOTF_HLG(double _input, double gamma = 1.2);

	double3 from_bt2020_to_XYZ(double3 x);

	double3 from_XYZ_to_bt2020(double3 x);

	double3 from_XYZ_to_sRGB(double3 x);

	double3 from_sRGB_to_XYZ(double3 x);

	double2 XYZ_to_xy(const double3& a);

	constexpr double3x3 to_XYZ(
		const double2& red_xy,
		const double2& green_xy,
		const double2& blue_xy,
		const double2& white_xy
	)
	{
		double3 r(red_xy.x, red_xy.y, 1.0 - (red_xy.x + red_xy.y));
		double3 g(green_xy.x, green_xy.y, 1.0 - (green_xy.x + green_xy.y));
		double3 b(blue_xy.x, blue_xy.y, 1.0 - (blue_xy.x + blue_xy.y));
		double3 w(white_xy.x, white_xy.y, 1.0 - (white_xy.x + white_xy.y));

		w /= white_xy.y;

		double3x3 retMat(r, g, b);

		double3x3 invMat;
		invMat = linalg::inverse(retMat);

		double3 scale = linalg::mul(invMat, w);

		retMat.x *= scale.x;
		retMat.y *= scale.y;
		retMat.z *= scale.z;

		return retMat;
	};

	double3 xyz_to_lab(double3 xyz);

	double3 lab_to_xyz(double3 lab);

	double3 lab_to_lch(double3 lab);

	double3 lch_to_lab(double3 lch);

	double3 xyz_to_lch(double3 xyz);

	double3 lch_to_xyz(double3 lch);

	byte3 to_byte3(const double3& v);

	int3 to_int3(const byte3& v);

	int3 to_int3(const double3& v);

	double3 to_double3(const byte3& v);

	void trim01(double3& input);

	QString vecToString(const double2& v);

	QString vecToString(const double3& v);

	QString vecToString(const double4& v);

	QString vecToString(const byte3& v);

	QString vecToString(const int3& v);

	QString matToString(double4x4 m);

	QString matToString(double3x3 m);

	

	void serialize(std::stringstream& out, const double2& v);

	void serialize(std::stringstream& out, const double3& v);

	void serialize(std::stringstream& out, const double4& v);


	void serialize(std::stringstream& out, const double4x4& m);

	void serialize(std::stringstream& out, const double3x3& m);

};

