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

#include <linalg.h>
#include <concepts>

struct ColorRgb;

namespace ColorSpaceMath
{
	using namespace linalg;
	using namespace aliases;

	enum class HDR_GAMMA : int { PQ = 0, HLG, sRGB, BT2020inSRGB, PQinSRGB, P010 };
	enum PRIMARIES { SRGB = 0, BT_2020, WIDE_GAMMUT };

	QString gammaToString(HDR_GAMMA gamma);

	constexpr float3x3 matrixF(const std::array<float, 9>& m) {
		return float3x3(
			float3(m[0], m[3], m[6]),
			float3(m[1], m[4], m[7]),
			float3(m[2], m[5], m[8])
		);
	}

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

	constexpr float3x3 matrix_srgb_to_bt709 = matrixF({
		0.2126f,  0.7152f,  0.0722f,
	   -0.1146f, -0.3854f,  0.5000f,
		0.5000f, -0.4542f, -0.0458f
		});
	
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

	constexpr float3x3 oklabM1 = matrixF({
		+0.4122214708f, +0.5363325363f, +0.0514459929f,
		+0.2119034982f, +0.6806995451f, +0.1073969566f,
		+0.0883024619f, +0.2817188376f, +0.6299787005f
		});

	constexpr float3x3 oklabM2 = matrixF({
		+0.2104542553f, +0.7936177850f, -0.0040720468f,
		+1.9779984951f, -2.4285922050f, +0.4505937099f,
		+0.0259040371f, +0.7827717662f, -0.8086757660f
		});

	constexpr float3x3 oklabInvM2 = matrixF({
		+1.0f, +0.3963377774f, +0.2158037573f,
		+1.0f, -0.1055613458f, -0.0638541728f,
		+1.0f, -0.0894841775f, -1.2914855480f
		});

	constexpr float3x3 oklabInvM1 = matrixF({
		+4.0767416621f, -3.3077115913f, +0.2309699292f,
		-1.2684380046f, +2.6097574011f, -0.3413193965f,
		-0.0228834462f, -0.7034186147f, +1.7263020608f
		});

	std::vector<double2> getPrimaries(PRIMARIES primary);

	mat<double, 3, 3> getPrimariesToXYZ(PRIMARIES primary);

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


	constexpr float3 rgb_to_bt709(const aliases::float3& rgb)
	{
		return linalg::mul(matrix_srgb_to_bt709, rgb);
	}

	constexpr float3 bt709_to_rgb(const aliases::float3& yuv)
	{
		constexpr float3x3 m = inverse(matrix_srgb_to_bt709);
		return linalg::mul(m, yuv);
	}

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

	double3 xyz_to_lab(const double3& xyz);

	double3 lab_to_xyz(const double3& lab);

	double3 lab_to_lch(const double3& lab);

	double3 lch_to_lab(double3 lch);

	double3 xyz_to_lch(const double3& xyz);

	double3 lch_to_xyz(const double3& lch);

	template<typename T>
	constexpr linalg::vec<T, 3> rgb2hsv(const linalg::vec<T, 3>& rgb)
	{
		using std::max;
		using std::min;
		using std::fmod;

		linalg::vec<T, 3> hsv;
		const T cmax = max({ rgb.x, rgb.y, rgb.z });
		const T cmin = min({ rgb.x, rgb.y, rgb.z });
		const T diff = cmax - cmin;

		if (cmax == cmin) {
			hsv.x = T(0);
		}
		else if (cmax == rgb.x) {
			hsv.x = fmod(T(60) * ((rgb.y - rgb.z) / diff) + T(360), T(360));
		}
		else if (cmax == rgb.y) {
			hsv.x = fmod(T(60) * ((rgb.z - rgb.x) / diff) + T(120), T(360));
		}
		else {
			hsv.x = fmod(T(60) * ((rgb.x - rgb.y) / diff) + T(240), T(360));
		}

		hsv.y = (cmax <= std::numeric_limits<T>::epsilon()) ? T(0) : (diff / cmax);
		hsv.z = cmax;
		return hsv;
	}

	template<typename T>
	constexpr linalg::vec<T, 3> hsv2rgb(const linalg::vec<T, 3>& hsv)
	{
		using std::fmod;
		using std::fabs;

		const T c = hsv.z * hsv.y;
		const T x_val = c * (T(1) - fabs(fmod(hsv.x / T(60), T(2)) - T(1)));
		const T m = hsv.z - c;
		linalg::vec<T, 3> rgb_prime;

		switch (static_cast<int>(hsv.x / T(60)))
		{
			case 0: rgb_prime = { c, x_val, T(0) }; break;
			case 1: rgb_prime = { x_val, c, T(0) }; break;
			case 2: rgb_prime = { T(0), c, x_val }; break;
			case 3: rgb_prime = { T(0), x_val, c }; break;
			case 4: rgb_prime = { x_val, T(0), c }; break;
			default: rgb_prime = { c, T(0), x_val }; break;
		}
		return { rgb_prime.x + m, rgb_prime.y + m, rgb_prime.z + m };
	}

	float3 clamp_oklab_chroma_to_gamut(const float3& oklab);

	float3 linear_rgb_to_oklab(const float3& c);

	float3 oklab_to_linear_rgb(const float3& lab);

	void test_oklab();

	double2 primaryRotateAndScale(const double2 primary,
		const double scaling,
		const double rotation,
		const std::vector<double2>& primaries,
		bool truncate = false);

	template<typename V>
	concept FloatOrDouble3 =
		std::same_as<V, float3> || std::same_as<V, double3>;

	template<typename V>
	concept ByteOrInt3 =
		std::same_as<V, byte3> || std::same_as<V, int3>;

	template<ByteOrInt3 OutVec, FloatOrDouble3 V>
	constexpr OutVec round_to_0_255(const V& v)
	{
		using TypeIn = decltype(v.x);
		using TypeOut = decltype(OutVec::x);

		return OutVec{
			static_cast<TypeOut>(std::lround(std::clamp(v.x, TypeIn(0), TypeIn(255)))),
			static_cast<TypeOut>(std::lround(std::clamp(v.y, TypeIn(0), TypeIn(255)))),
			static_cast<TypeOut>(std::lround(std::clamp(v.z, TypeIn(0), TypeIn(255))))
		};
	}

	int3 to_int3(const byte3& v);

	int3 to_int3(const double3& v);

	double3 to_double3(const byte3& v);

	template<typename T, int N>
	constexpr auto clamp01(const linalg::vec<T, N>& v)
	{
		return linalg::clamp(v, T(0), T(1));
	}

	template<typename T, int N>
	QString vecToString(const linalg::vec<T, N>& v)
	{
		QStringList parts;
		for (int i = 0; i < N; ++i) {
			if constexpr (std::is_integral_v<T>) {
				parts << QString("%1").arg(v[i], 3);
			}
			else {
				parts << QString::number(v[i], 'f', 3);
			}
		}
		return "[ " + parts.join(" ") + " ]";
	}

	template<typename T, int M, int N>
	QString matToString(const linalg::mat<T, M, N>& m)
	{
		QStringList rows;
		for (int r = 0; r < M; ++r)
			rows << vecToString(m.row(r));
		return rows.join("\r\n");
	}

	byte3 colorRgbToByte3(ColorRgb* rgb);

	template<typename T, int N>
	void serialize(std::stringstream& out, const linalg::vec<T, N>& v)
	{
		out << "{";
		for (int i = 0; i < N; i++)
		{
			if (i != 0) out << ", ";
			out << v[i];
		}
		out << "}";
	}

	template<typename T, int R, int C>
	void serialize(std::stringstream& out, const linalg::mat<T, R, C>& m)
	{
		out << "{";
		for (int i = 0; i < R; i++)
		{
			if (i != 0) out << ", ";
			serialize(out, m[i]);
		}
		out << "}";
	}
};

