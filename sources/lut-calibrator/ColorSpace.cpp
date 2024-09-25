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

#include <QList>
#include <QStringList>
#include <lut-calibrator/ColorSpace.h>

using namespace linalg;
using namespace aliases;

namespace ColorSpaceMath
{
	const std::map<PRIMARIES, std::vector<double2>> knownPrimaries = {
				{
					PRIMARIES::SRGB,
					{
						{ 0.6400, 0.3300 },
						{ 0.3000, 0.6000 },
						{ 0.1500, 0.0600 },
						{ 0.3127, 0.3290 }
					}
				},
				{
					PRIMARIES::BT_2020,
					{
						{ 0.708, 0.292 },
						{ 0.170, 0.797 },
						{ 0.131, 0.046 },
						{ 0.3127, 0.3290 }
					}
				},
				{
					PRIMARIES::WIDE_GAMMUT,
					{
						{ 0.7350, 0.2650 },
						{ 0.1150, 0.8260 },
						{ 0.1570, 0.0180 },
						{ 0.3127, 0.3290 }
					}
				}
	};

	QString gammaToString(HDR_GAMMA gamma)
	{
		if (gamma == HDR_GAMMA::PQ)
			return "PQ";
		else if (gamma == HDR_GAMMA::HLG)
			return "HLG";
		else if (gamma == HDR_GAMMA::sRGB)
			return "sRGB";
		else if (gamma == HDR_GAMMA::BT2020inSRGB)
			return "BT2020 with sRGB TRC";
		return "UNKNOWN";
	}

	double bt2020_nonlinear_to_linear(double input)
	{
		const double alpha = 1.09929682680944;
		const double beta = 0.018053968510807;

		if (input < 0) return -bt2020_nonlinear_to_linear(-input);
		if (input < beta * 4.5)
			return input / 4.5;
		else
			return std::pow((input + alpha - 1) / alpha, 1 / 0.45);
	}

	double bt2020_linear_to_nonlinear(double input)
	{
		const double alpha = 1.09929682680944;
		const double beta = 0.018053968510807;

		if (input < 0) return -bt2020_linear_to_nonlinear(-input);

		if (input < beta)
			return 4.5 * input;
		else
			return alpha * std::pow(input, 0.45) - (alpha - 1);
	}

	double3 bt2020_nonlinear_to_linear(double3 input)
	{
		return double3(bt2020_nonlinear_to_linear(input[0]),
			bt2020_nonlinear_to_linear(input[1]),
			bt2020_nonlinear_to_linear(input[2]));
	}

	double3 bt2020_linear_to_nonlinear(double3 input)
	{
		return double3(bt2020_linear_to_nonlinear(input[0]),
			bt2020_linear_to_nonlinear(input[1]),
			bt2020_linear_to_nonlinear(input[2]));
	}

	double srgb_nonlinear_to_linear(double input)
	{
		constexpr float alpha = 1.055010718947587f;
		constexpr float beta = 0.003041282560128f;

		input = std::max(input, 0.0);

		if (input < 12.92 * beta)
			input = input / 12.92f;
		else
			input = std::pow((input + (alpha - 1.0)) / alpha, 2.4);

		return input;
	}

	double3 srgb_nonlinear_to_linear(double3 input)
	{
		return double3(srgb_nonlinear_to_linear(input[0]),
			srgb_nonlinear_to_linear(input[1]),
			srgb_nonlinear_to_linear(input[2]));
	}

	double srgb_linear_to_nonlinear(double input)
	{
		constexpr float alpha = 1.055010718947587f;
		constexpr float beta = 0.003041282560128f;

		input = std::max(input, 0.0);

		if (input < beta)
			input = input * 12.92;
		else
			input = alpha * std::pow(input, 1.0 / 2.4) - (alpha - 1.0);

		return input;
	}

	double3 srgb_linear_to_nonlinear(double3 input)
	{
		return double3(srgb_linear_to_nonlinear(input[0]),
			srgb_linear_to_nonlinear(input[1]),
			srgb_linear_to_nonlinear(input[2]));
	}

	double3 from_BT2020_to_BT709(double3 a)
	{
		double3 b;
		b.x = 1.6605 * a.x - 0.5876 * a.y - 0.0728 * a.z;
		b.y = -0.1246 * a.x + 1.1329 * a.y - 0.0083 * a.z;
		b.z = -0.0182 * a.x - 0.1006 * a.y + 1.1187 * a.z;
		return b;
	}

	linalg::mat<double, 3, 3> getPrimariesToXYZ(PRIMARIES primary)
	{
		const auto& primaries = knownPrimaries.at(primary);
		return to_XYZ(primaries[0], primaries[1], primaries[2], primaries[3]);
	}

	double PQ_ST2084(double scale, double  nonlinear)
	{
		constexpr double M1 = (2610.0 / 16384.0);
		constexpr double M2 = (2523.0 / 4096.0) * 128.0;
		constexpr double C1 = (3424.0 / 4096.0);
		constexpr double C2 = (2413.0 / 4096.0) * 32.0;
		constexpr double C3 = (2392.0 / 4096.0) * 32.0;

		if (nonlinear > 0.0)
		{
			double xpow = std::pow(nonlinear, 1.0 / M2);
			double num = std::max(xpow - C1, 0.0);
			double den = std::max(C2 - C3 * xpow, DBL_MIN);
			nonlinear = std::pow(num / den, 1.0 / M1);
		}
		else if (nonlinear < 0.0)
		{
			return -PQ_ST2084(scale, -nonlinear);
		}
		else
			return 0;

		return scale * nonlinear;
	}

	double3 PQ_ST2084(double scale, double3 nonlinear)
	{
		return double3(PQ_ST2084(scale, nonlinear[0]),
			PQ_ST2084(scale, nonlinear[1]),
			PQ_ST2084(scale, nonlinear[2]));
	}

	double inverse_OETF_HLG(double t)
	{
		if (t < 0) return -inverse_OETF_HLG(-t);

		constexpr double a = 0.17883277;
		constexpr double b = 0.28466892;
		constexpr double c = 0.55991073;
		constexpr double one_twelfth = 1.0 / 12.0;

		if (t < 0.5) return t * t / 3.0;
		else return (std::exp((t - c) / a) + b) * one_twelfth;
	};

	double3 inverse_OETF_HLG(double3 input)
	{
		return double3(inverse_OETF_HLG(input[0]),
			inverse_OETF_HLG(input[1]),
			inverse_OETF_HLG(input[2]));
	}

	double3 OOTF_HLG(double3 input, double gamma)
	{
		if (gamma == 0)
			return input;

		double3 coefs{ 0.2627, 0.6780, 0.0593 };
		double luma = linalg::dot(input, coefs);
		luma = linalg::pow(luma, gamma - 1.0);

		return input * luma;
	}

	double3 OOTF_HLG(double _input, double gamma)
	{
		double3 input(_input);

		if (gamma == 0)
			return input;

		return OOTF_HLG(input, gamma);
	}

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
			std::lround(std::max(std::min(v.x, 255.0), 0.0)),
			std::lround(std::max(std::min(v.y, 255.0), 0.0)),
			std::lround(std::max(std::min(v.z, 255.0), 0.0))
		);
	}

	int3 to_int3(const byte3& v)
	{
		return int3(v.x, v.y, v.z);
	}

	int3 to_int3(const double3& v)
	{
		return int3(std::lround(v.x), std::lround(v.y), std::lround(v.z));
	}

	double3 to_double3(const byte3& v)
	{
		return double3(v.x, v.y, v.z);
	}

	void trim01(double3& input)
	{
		input.x = std::max(0.0, std::min(input.x, 1.0));
		input.y = std::max(0.0, std::min(input.y, 1.0));
		input.z = std::max(0.0, std::min(input.z, 1.0));
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

	QString vecToString(const int3& v)
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

	void serialize(std::stringstream& out, const double2& v)
	{
		out << "{" << v[0] << ", " << v[1] << "}";
	}

	void serialize(std::stringstream& out, const double3& v)
	{
		out << "{" << v[0] << ", " << v[1] << ", " << v[2] << "}";
	}

	void serialize(std::stringstream& out, const double4& v)
	{
		out << "{" << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << "}";
	}

	void serialize(std::stringstream& out, const double4x4& m)
	{
		out << "{";
		for (int d = 0; d < 4; d++)
		{
			if (d != 0) out << ", ";
			serialize(out, m[d]);
		}
		out << "}";
	}

	void serialize(std::stringstream& out, const double3x3& m)
	{
		out << "{";
		for (int d = 0; d < 3; d++)
		{
			if (d != 0) out << ", ";
			serialize(out, m[d]);			
		}
		out << "}";
	}

};

