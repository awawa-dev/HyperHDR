#pragma once

/* ColorSpace.h
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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

#include <lut-calibrator/VectorHelper.h>
#include <lut-calibrator/LutCalibrator.h>

namespace ColorSpaceMath
{
	enum PRIMARIES { SRGB = 0, BT_2020, WIDE_GAMMUT };

	QString gammaToString(HDR_GAMMA gamma);

	const double3x3 matrix(std::array<double, 9> m);
	const double4x4 matrix4(const std::array<double, 16>& m);
	double3x3 matrix_bt2020_to_XYZ();
	double3x3 matrix_sRgb_to_XYZ();

	std::vector<double2> getPrimaries(PRIMARIES primary);

	double3x3 getPrimariesToXYZ(PRIMARIES primary);

	double3 bt2020_nonlinear_to_linear(double3 input);

	double3 bt2020_linear_to_nonlinear(double3 input);

	double srgb_nonlinear_to_linear(double input);

	double3 srgb_nonlinear_to_linear(double3 input);
	
	double3 srgb_linear_to_nonlinear(double3 input);

	double srgb_linear_to_nonlinear(double input);

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

	inline double3x3 to_XYZ(
		const double2& red_xy,
		const double2& green_xy,
		const double2& blue_xy,
		const double2& white_xy
	)
	{
		double3 r(red_xy.x(), red_xy.y(), 1.0 - (red_xy.x() + red_xy.y()));
		double3 g(green_xy.x(), green_xy.y(), 1.0 - (green_xy.x() + green_xy.y()));
		double3 b(blue_xy.x(), blue_xy.y(), 1.0 - (blue_xy.x() + blue_xy.y()));
		double3 w(white_xy.x(), white_xy.y(), 1.0 - (white_xy.x() + white_xy.y()));

		w /= white_xy.y();

		double3x3 retMat;
		retMat.col(0) = r;
		retMat.col(1) = g;
		retMat.col(2) = b;

		double3x3 invMat = retMat.inverse();

		double3 scale = invMat * w;

		retMat.col(0) *= scale.x();
		retMat.col(1) *= scale.y();
		retMat.col(2) *= scale.z();

		return retMat;
	};

	double3 xyz_to_lab(const double3& xyz);

	double3 lab_to_xyz(const double3& lab);

	double3 lab_to_lch(const double3& lab);

	double3 lch_to_lab(double3 lch);

	double3 xyz_to_lch(const double3& xyz);

	double3 lch_to_xyz(const double3& lch);

	double3 rgb2hsv(double3 rgb);

	double3 hsv2rgb(double3 hsv);

	float3 rgb2hsv(float3 rgb);

	float3 hsv2rgb(float3 hsv);

	double2 primaryRotateAndScale(const double2 primary,
		const double scaling,
		const double rotation,
		const std::vector<double2>& primaries,
		bool truncate = false);

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

