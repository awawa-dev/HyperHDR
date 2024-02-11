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
#endif

#include <linalg.h>

using namespace linalg;
using namespace aliases;

namespace ColorSpaceMath
{
	enum PRIMARIES { SRGB = 0 };

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

class YuvConverter
{

public:
	enum YUV_COEFS { FCC = 0, BT601 = 1, BT709 = 2, BT2020 = 3 };
	enum COLOR_RANGE { FULL = 0, LIMITED = 1 };
	enum YUV_DIRECTION { FROM_RGB_TO_YUV = 0, FROM_YUV_TO_RGB = 1 };

	const std::map<YUV_COEFS, double2> knownCoeffs = {
				{YUV_COEFS::FCC,      {0.3,    0.11  } },
				{YUV_COEFS::BT601,    {0.2990, 0.1140} },
				{YUV_COEFS::BT709,    {0.2126, 0.0722} },
				{YUV_COEFS::BT2020,   {0.2627, 0.0593} }
	};

	double3 toRgb(COLOR_RANGE range, YUV_COEFS coef, const double3& input) const
	{
		double4 ret(input, 1);
		ret = mul(yuv2rgb.at(range).at(coef), ret);
		return double3(ret.x, ret.y, ret.z);
	}

	double3 toYuvBT709(COLOR_RANGE range, const double3& input) const
	{
		double4 ret(input, 1);
		ret = mul(rgb2yuvBT709.at(range), ret);
		return double3(ret.x, ret.y, ret.z);
	}

	QString coefToString(YUV_COEFS cf)
	{
		switch (cf)
		{
			case(FCC):		return "FCC";		break;
			case(BT601):	return "BT601";		break;
			case(BT709):	return "BT709";		break;
			case(BT2020):	return "BT2020";	break;
			default:		return "?";
		}
	}

	YuvConverter()
	{
		for (const auto& coeff : knownCoeffs)
			for (const COLOR_RANGE& range : { COLOR_RANGE::FULL,  COLOR_RANGE::LIMITED })
			{
				const double Kr = coeff.second.x;
				const double Kb = coeff.second.y;
				const double Kg = 1.0 - Kr - Kb;
				const double Cr = 0.5 / (1.0 - Kb);
				const double Cb = 0.5 / (1.0 - Kr);

				double scaleY = 1.0, addY = 0.0, scaleUV = 1.0, addUV = 128 / 255.0;

				if (range == COLOR_RANGE::LIMITED)
				{
					scaleY = 219 / 255.0;
					addY = 16 / 255.0;
					scaleUV = 224 / 255.0;
				}

				double4 c1(Kr * scaleY, -Kr * Cr * scaleUV, (1 - Kr) * Cb * scaleUV, 0);
				double4 c2(Kg * scaleY, -Kg * Cr * scaleUV, -Kg * Cb * scaleUV, 0);
				double4 c3(Kb * scaleY, (1 - Kb) * Cr * scaleUV, -Kb * Cb * scaleUV, 0);
				double4 c4(addY, addUV, addUV, 1);

				double4x4 rgb2yuvMatrix(c1, c2, c3, c4);

				double4x4 yuv2rgbMatrix = inverse(rgb2yuvMatrix);

				yuv2rgb[range][coeff.first] = yuv2rgbMatrix;

				if (coeff.first == YUV_COEFS::BT709)
				{
					rgb2yuvBT709[range] = rgb2yuvMatrix;
				}
			}

	}

	QString toString()
	{
		QStringList ret, report;

		for (const auto& coeff : knownCoeffs)
			for (const COLOR_RANGE& range : { COLOR_RANGE::FULL,  COLOR_RANGE::LIMITED })
			{
				double4x4 matrix = yuv2rgb[range][coeff.first];
				ret.append(QString("YUV to RGB %1 (%2):").arg(coefToString(coeff.first), 6).arg((range == COLOR_RANGE::LIMITED) ? "Limited" : "Full"));
				ret.append(ColorSpaceMath::matToString(matrix).split("\r\n"));
			}

		for (const COLOR_RANGE& range : { COLOR_RANGE::FULL,  COLOR_RANGE::LIMITED })
		{
			ret.append(QString("RGB to YUV %1 (%2):").arg(coefToString(YUV_COEFS::BT709), 6).arg((range == COLOR_RANGE::LIMITED) ? "Limited" : "Full"));
			ret.append(ColorSpaceMath::matToString(rgb2yuvBT709[range]).split("\r\n"));
		}

		for (int i = 0; i + 5 < ret.size(); i += 5)
			for (int j = 0; j < 5; j++)
				report.append(QString("%1 %2").arg(ret[i], -32).arg(ret[(i++) + 5], -32));

		return "Supported YUV/RGB matrix transformation:\r\n\r\n" + report.join("\r\n");
	}



private:
	std::map<COLOR_RANGE, std::map<YUV_COEFS, double4x4>> yuv2rgb;
	std::map<COLOR_RANGE, double4x4> rgb2yuvBT709;
};
