/* ColorSpace.cpp
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

#include <lut-calibrator/ColorSpace.h>

using namespace linalg;
using namespace aliases;

namespace ColorSpaceMath
{
	const std::map<PRIMARIES, std::vector<double3>> knownPrimaries = {
				{
					PRIMARIES::SRGB,
					{
						{ 0.6400, 0.3300, 0.2126 },
						{ 0.3000, 0.6000, 0.7152 },
						{ 0.1500, 0.0600, 0.0722 },
						{ 0.3127, 0.3290, 1.0000 }
					}
				}
	};

	double3 from_bt2020_to_XYZ(double3 x)
	{
		return mul(matrix_bt2020_to_XYZ, x);
	}

	double3 from_XYZ_to_bt2020(double3 x)
	{
		constexpr double3x3 m = inverse(matrix_bt2020_to_XYZ);
		return mul(m, x);
	}

	double3 from_XYZ_to_sRGB(double3 x)
	{
		constexpr double3x3 m = inverse(matrix_sRgb_to_XYZ);
		return mul(m, x);
	}

	double3 from_sRGB_to_XYZ(double3 x)
	{
		return mul(matrix_sRgb_to_XYZ, x);
	}

	double2 XYZ_to_xy(const double3& a)
	{
		double len = std::max(a.x + a.y + a.z, std::numeric_limits<double>::epsilon());
		return { a.x / len, a.y / len };
	}

	double3x3 to_XYZ(
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

		retMat[0] *= scale.x;
		retMat[1] *= scale.y;
		retMat[2] *= scale.z;

		return retMat;
	}


	double3 xyz_to_lab(double3 xyz)
	{
		double x = xyz.x / 95.047;
		double y = xyz.y / 100.00;
		double z = xyz.z / 108.883;

		x = (x > 0.008856) ? std::cbrt(x) : (7.787 * x + 16.0 / 116.0);
		y = (y > 0.008856) ? std::cbrt(y) : (7.787 * y + 16.0 / 116.0);
		z = (z > 0.008856) ? std::cbrt(z) : (7.787 * z + 16.0 / 116.0);

		return double3(
			(116.0 * y) - 16,
			500 * (x - y),
			200 * (y - z)
		);
	}

	double3 lab_to_xyz(double3 lab)
	{
		double y = (lab.x + 16.0) / 116.0;
		double x = lab.y / 500.0 + y;
		double z = y - lab.z / 200.0;

		double x3 = std::pow(x, 3);
		double y3 = std::pow(y, 3);
		double z3 = std::pow(z, 3);

		return double3(
			x = ((x3 > 0.008856) ? x3 : ((x - 16.0 / 116.0) / 7.787)) * 95.047,
			y = ((y3 > 0.008856) ? y3 : ((y - 16.0 / 116.0) / 7.787)) * 100.0,
			z = ((z3 > 0.008856) ? z3 : ((z - 16.0 / 116.0) / 7.787)) * 108.883
		);
	}

	double3 lab_to_lch(double3 lab)
	{
		auto l = lab.x;
		auto a = lab.y;
		auto b = lab.z;

		const auto c = std::sqrt(std::pow(a, 2) + std::pow(b, 2));

		auto h = std::atan2(b, a);

		if (h > 0)
		{
			h = (h / M_PI) * 180.0;
		}
		else
		{
			h = 360.0 - (std::abs(h) / M_PI) * 180.0;
		}

		return double3(l, c, h);
	}

	double3 lch_to_lab(double3 lch)
	{
		if (lch.z > 360.0)
			lch.z -= 360.0;
		else if (lch.z < 360.0)
			lch.z += 360.0;

		double h = lch.z * M_PI / 180.0;

		return double3(
			lch.x,
			std::cos(h) * lch.y,
			std::sin(h) * lch.y);
	}

	double3 xyz_to_lch(double3 xyz)
	{
		xyz = xyz_to_lab(xyz);
		return lab_to_lch(xyz);
	}

	double3 lch_to_xyz(double3 lch)
	{
		lch = lch_to_lab(lch);
		return lab_to_xyz(lch);
	}

	byte3 to_byte3(const double3& v)
	{
		return byte3(
			std::round(std::max(std::min(v.x, 255.0), 0.0)),
			std::round(std::max(std::min(v.y, 255.0), 0.0)),
			std::round(std::max(std::min(v.z, 255.0), 0.0))
		);
	}

	double3 to_double3(const byte3& v)
	{
		return double3(v.x, v.y, v.z);
	}

	void trim01(double3& input)
	{
		for (int i = 0; i < 3; i++)
		{
			if (input[i] > 1.0)
				input[i] = 1.0;
			else if (input[i] < 0.0)
				input[i] = 0.0;
		}
	}

	QString vecToString(const double2& v)
	{
		return QString("[%1 %2]").arg(v[0], 7, 'f', 3).arg(v[1], 7, 'f', 3);
	}

	QString vecToString(const double3& v)
	{
		return QString("[%1 %2 %3]").arg(v[0], 7, 'f', 3).arg(v[1], 7, 'f', 3).arg(v[2], 7, 'f', 3);
	}

	QString vecToString(const double4& v)
	{
		return QString("[%1 %2 %3 %4]").arg(v[0], 6, 'f', 3).arg(v[1], 6, 'f', 3).arg(v[2], 6, 'f', 3).arg(v[3], 6, 'f', 3);
	}

	QString vecToString(const byte3& v)
	{
		return QString("[%1 %2 %3]").arg(v[0], 3).arg(v[1], 3).arg(v[2], 3);
	}

	QString matToString(double4x4 m)
	{
		QStringList ret;
		for (int d = 0; d < 4; d++)
		{
			ret.append(vecToString(m.row(d)));
		}
		return ret.join("\r\n");
	}

	QString matToString(double3x3 m)
	{
		QStringList ret;
		for (int d = 0; d < 3; d++)
		{
			ret.append(vecToString(m.row(d)));
		}
		return ret.join("\r\n");
	}
};

