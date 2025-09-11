/* ColorSpace.cpp
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

#include <chrono>
#include <QList>
#include <QStringList>
#include <image/ColorRgb.h>
#include <infinite-color-engine/ColorSpace.h>
#include <numbers>

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
		else if (gamma == HDR_GAMMA::PQinSRGB)
			return "PQ in SRGB";
		else if (gamma == HDR_GAMMA::P010)
			return "P010";
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

	std::vector<double2> getPrimaries(PRIMARIES primary)
	{
		return knownPrimaries.at(primary);
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


	double3 xyz_to_lab(const double3& xyz)
	{
		double x = xyz.x / 95.047;
		double y = xyz.y / 100.00;
		double z = xyz.z / 108.883;

		x = (x > 0.008856) ? std::cbrt(x) : (7.787 * x + 16.0 / 116.0);
		y = (y > 0.008856) ? std::cbrt(y) : (7.787 * y + 16.0 / 116.0);
		z = (z > 0.008856) ? std::cbrt(z) : (7.787 * z + 16.0 / 116.0);

		return double3{
			(116.0 * y) - 16,
			500 * (x - y),
			200 * (y - z)
		};
	}

	double3 lab_to_xyz(const double3& lab)
	{
		double y = (lab.x + 16.0) / 116.0;
		double x = lab.y / 500.0 + y;
		double z = y - lab.z / 200.0;

		double x3 = std::pow(x, 3);
		double y3 = std::pow(y, 3);
		double z3 = std::pow(z, 3);

		return double3{
			x = ((x3 > 0.008856) ? x3 : ((x - 16.0 / 116.0) / 7.787)) * 95.047,
			y = ((y3 > 0.008856) ? y3 : ((y - 16.0 / 116.0) / 7.787)) * 100.0,
			z = ((z3 > 0.008856) ? z3 : ((z - 16.0 / 116.0) / 7.787)) * 108.883
		};
	}

	double3 lab_to_lch(const double3& lab)
	{
		const auto& l = lab.x;
		const auto& a = lab.y;
		const auto& b = lab.z;

		const auto c = std::sqrt(std::pow(a, 2) + std::pow(b, 2));

		auto h = std::atan2(b, a);

		if (h > 0)
		{
			h = (h / std::numbers::pi) * 180.0;
		}
		else
		{
			h = 360.0 - (std::abs(h) / std::numbers::pi) * 180.0;
		}

		return double3{ l, c, h };
	}

	double3 lch_to_lab(double3 lch)
	{
		if (lch.z > 360.0)
			lch.z -= 360.0;
		else if (lch.z < 360.0)
			lch.z += 360.0;

		double h = lch.z * std::numbers::pi / 180.0;

		return double3{
			lch.x,
			std::cos(h) * lch.y,
			std::sin(h) * lch.y };
	}

	double3 xyz_to_lch(const double3& xyz)
	{
		return lab_to_lch(xyz_to_lab(xyz));
	}

	double3 lch_to_xyz(const double3& lch)
	{
		return lab_to_xyz(lch_to_lab(lch));
	}

	int3 round(const double3& v)
	{
		return int3(std::lround(v.x), std::lround(v.y), std::lround(v.z));
	}

	byte3 colorRgbToByte3(ColorRgb* rgb)
	{
		return byte3(rgb->red, rgb->green, rgb->blue);
	}

	constexpr double intersectSegments(const double2& p1, const double2& p2, const double2& p3, const double2& p4)
	{
		const double denominator = linalg::determinant( double2x2{ {p1.x - p2.x, p3.x - p4.x}, {p1.y - p2.y, p3.y - p4.y} });
		if (denominator == 0.0)
			return DBL_MAX;

		const double t = linalg::determinant( double2x2{ {p1.x - p3.x, p3.x - p4.x}, {p1.y - p3.y, p3.y - p4.y} } ) / denominator;
		if (t >= 0.0)
			return t;
		return DBL_MAX;
	}

	double maxLenInColorspace
	(const std::vector<double2>& primaries,
		const double cos_angle,
		const double sin_angle)
	{
		const double2& p1 = primaries[3];

		const double2 p2 = double2{ p1.x + cos_angle, p1.y + sin_angle };

		double distance_to_edge = DBL_MAX;
		for (size_t i = 0; i < 3; i++)
		{
			const size_t nextPrimary = (i == 2) ? 0 : (i + 1);
			const double2& p3 = primaries[i];
			const double2& p4 = primaries[nextPrimary];
			const float distance = intersectSegments(p1, p2, p3, p4);
			if (distance < distance_to_edge)
				distance_to_edge = distance;
		}

		return distance_to_edge;
	}

	double2 primaryRotateAndScale(const double2 primary,
		const double scaling,
		const double rotation,
		const std::vector<double2>& primaries,
		bool truncate)
	{
		const double2 d = primary - primaries[3];
		const double angle = std::atan2(d.y, d.x) + rotation;
		const double cos_angle = std::cos(angle);
		const double sin_angle = std::sin(angle);
		const double2 dx = double2{ cos_angle, sin_angle } * scaling * ((truncate) ? maxLenInColorspace(primaries, cos_angle, sin_angle)  : linalg::length(d));

		return dx + primaries[3];
	}

	inline float ufast_cbrt(float x)
	{
		union {
			int i;
			float f;
		} v;
		v.f = x;
		v.i = 0x2a5120a8 + v.i / 3;

		float y = v.f;
		y = y - (y - x / (y * y)) / 3.0f;
		return y;
	}
	
	float3 linear_rgb_to_oklab(const float3& rgb)
	{
		float3 lms = mul(oklabM1, rgb);
		float3 lms_cbrt = {
			ufast_cbrt(lms.x),
			ufast_cbrt(lms.y),
			ufast_cbrt(lms.z)
		};
		return mul(oklabM2, lms_cbrt);
	}

	float3 oklab_to_linear_rgb(const float3& lab)
	{
		float3 lms_cbrt = mul(oklabInvM2, lab);
		float3 lms = {
			lms_cbrt.x * lms_cbrt.x * lms_cbrt.x,
			lms_cbrt.y * lms_cbrt.y * lms_cbrt.y,
			lms_cbrt.z * lms_cbrt.z * lms_cbrt.z
		};
		return mul(oklabInvM1, lms);
	}
	
	float3 clamp_oklab_chroma_to_gamut(const float3& oklab)
	{
		float L = oklab.x;
		float a = oklab.y;
		float b = oklab.z;

		float chroma = std::sqrt(a * a + b * b);
		if (chroma < 1e-5f)
			return oklab; // już szary – w gamut

		float dir_a = a / chroma;
		float dir_b = b / chroma;

		// Oszacuj maksymalną chrominancję
		float lo = 0.0f;
		float hi = chroma;

		// 5 iteracji wystarczy dla dużej dokładności
		for (int i = 0; i < 5; ++i) {
			float mid = 0.5f * (lo + hi);
			float3 test = { L, dir_a * mid, dir_b * mid };
			float3 rgb = oklab_to_linear_rgb(test);

			if (rgb.x < 0.0f || rgb.x > 1.0f ||
				rgb.y < 0.0f || rgb.y > 1.0f ||
				rgb.z < 0.0f || rgb.z > 1.0f)
				hi = mid;
			else
				lo = mid;
		}

		float C_max = lo;

		return { L, dir_a * C_max, dir_b * C_max };
	}

	void test_oklab()
	{
		auto start = std::chrono::high_resolution_clock::now();
		constexpr int steps = 256;
		constexpr float step = 1.0f / 255.0f;

		double max_abs_error = 0.0;
		float3 rgb_max_error = { 0.f, 0.f, 0.f };

		for (int r = 0; r < steps; ++r) {
			for (int g = 0; g < steps; ++g) {
				for (int b = 0; b < steps; ++b) {
					float3 rgb{
						r * step,
						g * step,
						b * step
					};

					float3 lab = linear_rgb_to_oklab(rgb);
					float3 rgb_rec = oklab_to_linear_rgb(lab);

					float3 error = rgb_rec - rgb;

					error.x = std::abs(error.x) * 255.0f;
					error.y = std::abs(error.y) * 255.0f;
					error.z = std::abs(error.z) * 255.0f;

					float max_error = std::max({ error.x, error.y, error.z });

					if (max_error > max_abs_error) {
						max_abs_error = max_error;
						rgb_max_error = rgb;
					}
				}
			}
		}

		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "Sredni czas: " << (duration.count() / 1000.0) << " us\n";

		std::cout << "Max absolute error (in RGB 0-255 scale): " << max_abs_error << "\n";
		std::cout << "At RGB = (" << rgb_max_error.x * 255.0f << ", "
			<< rgb_max_error.y * 255.0f << ", "
			<< rgb_max_error.z * 255.0f << ")\n";

		
		std::cout << "-----------------------------------------\n";		
		auto approx_equal = [](const float3& a, const float3& b, float tol = 1e-4f) {
			for (int i = 0; i < 3; ++i) {
				if (std::fabs(a[i] - b[i]) > tol) return false;
			}
			return true;
			};
		auto to_string = [](const float3& v) {
			char buf[64];
			snprintf(buf, sizeof(buf), "(%.6f, %.6f, %.6f)", v.x, v.y, v.z);
			return std::string(buf);
			};

		struct TestPair {
			float3 linear_rgb;
			float3 expected_oklab;
			std::string name;
		};

		std::vector<TestPair> tests = {
			{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, "Black"},
			{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, "White"},
			{{0.5f, 0.5f, 0.5f}, {0.793701f, 0.0f, 0.0f}, "Gray 50%"},
			{{1.0f, 0.0f, 0.0f}, {0.627955f, 0.224863f, 0.125846f}, "Red"},
			{{0.0f, 1.0f, 0.0f}, {0.866440f, -0.233887f, 0.179498f}, "Green"},
			{{0.0f, 0.0f, 1.0f}, {0.452014f, -0.032457f, -0.311528f}, "Blue"},
			{{0.0f, 1.0f, 1.0f}, {0.905399f, -0.149444f, -0.039398f}, "Cyan"},
			{{1.0f, 0.0f, 1.0f}, {0.701674f, 0.274566f, -0.169156f}, "Magenta"},
			{{1.0f, 1.0f, 0.0f}, {0.967983f, -0.071369f, 0.198570f}, "Yellow"},
			{{1.0f, 0.38463f, 0.0f}, {0.795603f, 0.054130f, 0.162007f}, "Orange"},
			{{0.0573f, 0.1523f, 0.8616f}, {0.578790f, -0.011550f, -0.200742f}, "Royal Blue"},
			{{0.21786f, 0.0f, 0.21786f}, {0.422209f, 0.165211f, -0.101784f}, "Purple"}
		};

		// Pętla testująca z dwiema tolerancjami
		bool all_passed = true;
		const float forward_tol = 5e-3f;
		const float round_trip_tol = 1.5e-2f; // ~1.5% tolerancji

		for (size_t i = 0; i < tests.size(); ++i) {
			const auto& test = tests[i];
			float3 result_oklab = linear_rgb_to_oklab(test.linear_rgb);
			float3 result_oklab_inv = oklab_to_linear_rgb(result_oklab);

			bool forward_pass = approx_equal(result_oklab, test.expected_oklab, forward_tol);
			bool round_trip_pass = approx_equal(test.linear_rgb, result_oklab_inv, round_trip_tol);

			if (!forward_pass || !round_trip_pass) {
				std::cout << "Test " << i << " FAIL (" << test.name << ")\n";
				if (!forward_pass) {
					std::cout << " -> Blad konwersji w przod:\n";
					std::cout << "    Wejscie (RGB): " << to_string(test.linear_rgb) << "\n";
					std::cout << "    Oczekiwano (Oklab): " << to_string(test.expected_oklab) << "\n";
					std::cout << "    Otrzymano (Oklab):  " << to_string(result_oklab) << "\n";
				}
				if (!round_trip_pass) {
					std::cout << " -> Blad konwersji powrotnej:\n";
					std::cout << "    Oryginal (RGB): " << to_string(test.linear_rgb) << "\n";
					std::cout << "    Odwrocony (RGB):" << to_string(result_oklab_inv) << "\n";
				}
				std::cout << "\n";
				all_passed = false;
			}
			else {
				std::cout << "Test " << i << " PASS (" << test.name << ")\n";
			}
		}
		std::cout << "-----------------------------------------\n";
		struct TestCase {
			float3 input_oklab;
			float3 expected_linear_rgb;
		};

		auto rgb_close_enough = [](const float3& a, const float3& b, float epsilon = 0.01f) {
			return (std::abs(a.x - b.x) < epsilon) &&
				(std::abs(a.y - b.y) < epsilon) &&
				(std::abs(a.z - b.z) < epsilon);
			};

		std::vector<TestCase> testsClip = {
			{{0.70f, 0.50f, 0.40f}, {0.940749f, 0.156919f, 0.044987f}},
			{{0.60f, 0.45f, -0.40f}, {0.535061f, 0.007299f, 0.812949f}},
			{{0.85f, -0.50f, 0.25f}, {0.019331f, 0.901008f, 0.250915f}},
			{{0.50f, 0.60f, 0.60f}, {0.342420f, 0.059090f, 0.004047f}},
			{{0.75f, -0.25f, -0.55f}, {0.114831f, 0.464384f, 0.981130f}},
			{{0.90f, 0.65f, -0.45f}, {0.939548f, 0.615304f, 0.951606f}},
			{{0.65f, 0.50f, 0.50f}, {0.766004f, 0.125364f, 0.004126f}},
			{{0.80f, -0.35f, 0.55f}, {0.398878f, 0.625847f, 0.017660f}},
			{{0.55f, 0.48f, -0.48f}, {0.381601f, 0.012339f, 0.660164f}},
			{{0.78f, 0.44f, 0.44f}, {0.996280f, 0.321020f, 0.140910f}},
			{{0.82f, -0.46f, 0.40f}, {0.112118f, 0.800387f, 0.013063f}},
			{{0.68f, 0.52f, -0.35f}, {0.879337f, 0.003213f, 0.972316f}},
			{{0.59f, -0.42f, 0.45f}, {0.094345f, 0.276515f, 0.010003f}},
			{{0.73f, 0.58f, -0.50f}, {0.743215f, 0.168560f, 0.971310f}},
			{{0.85f, -0.40f, 0.30f}, {0.004038f, 0.939812f, 0.015236f}},
		};

		constexpr float epsilon = 0.02f; // Tolerancja błędu dla kolorów linear RGB

		all_passed = true;
		for (size_t i = 0; i < testsClip.size(); ++i) {
			const auto& testClip = testsClip[i];

			float3 clamped_oklab = clamp_oklab_chroma_to_gamut(testClip.input_oklab);
			float3 result_rgb = oklab_to_linear_rgb(clamped_oklab);

			bool pass = rgb_close_enough(result_rgb, testClip.expected_linear_rgb, epsilon);

			std::cout << "Test " << i << ": "
				<< (pass ? "PASS" : "FAIL") << "\n"
				<< "  Input Oklab: (" << testClip.input_oklab.x << ", " << testClip.input_oklab.y << ", " << testClip.input_oklab.z << ")\n"
				<< "  Expected linear RGB: (" << testClip.expected_linear_rgb.x << ", " << testClip.expected_linear_rgb.y << ", " << testClip.expected_linear_rgb.z << ")\n"
				<< "  Result linear RGB: (" << result_rgb.x << ", " << result_rgb.y << ", " << result_rgb.z << ")\n\n";

			if (!pass) all_passed = false;
		}

		if (all_passed)
			std::cout << "All tests passed.\n";
		else
			std::cout << "Some tests failed.\n";
	}
};

