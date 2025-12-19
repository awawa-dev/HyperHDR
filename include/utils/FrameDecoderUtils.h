/* FrameDecoderUtils.h
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


#include <vector>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <cstdlib>

class FrameDecoderUtils
{
public:
	static FrameDecoderUtils& instance()
	{
		static FrameDecoderUtils inst;
		return inst;
	}

	const std::array<uint8_t,1024>& getLutP010_y() const { return lutP010_y; }
	const std::array<uint8_t,1024>& getlutP010_uv() const { return lutP010_uv; }
	
	static constexpr double signalBreakP010 = 0.91;
	static constexpr double signalBreakChromaP010 = 0.75;

	static double packChromaP010(double x)
	{
		constexpr double pi2 = std::numbers::pi / 2.0;
		if (x < 0.0)
		{
			return 0.0;
		}
		else if (x <= 0.5)
		{
			return x * 1.5;
		}
		else if (x <= 1)
		{
			return std::sin(pi2 * ((x - 0.5) / 0.5)) * (1 - signalBreakChromaP010) + signalBreakChromaP010;
		}
		return 1;
	};

	static double unpackChromaP010(double x)
	{
		constexpr double pi2 = std::numbers::pi / 2.0;
		if (x < 0.0)
		{
			return 0.0;
		}
		else if (x <= signalBreakChromaP010)
		{
			x /= 1.5;
			return x;
		}
		else if (x <= 1)
		{
			x = (x - signalBreakChromaP010) / (1.0 - signalBreakChromaP010);
			x = std::asin(x);
			x = x * 0.5 / pi2 + 0.5;
			return x;
		}

		return 1;
	};

	static double packLuminanceP010(double x)
	{
		constexpr double pi2 = std::numbers::pi / 2.0;
		if (x < 0.0)
		{
			return 0.0;
		}
		else if (x <= 0.7)
		{
			return x * 1.3;
		}
		else if (x <= 1)
		{
			return std::sin(pi2 * ((x - 0.7) / 0.3)) * (1 - signalBreakP010) + signalBreakP010;
		}
		return 1;
	};

	static double unpackLuminanceP010(double x)
	{
		constexpr double pi2 = std::numbers::pi / 2.0;
		if (x < 0.0)
		{
			return 0.0;
		}
		else if (x <= signalBreakP010)
		{
			return x / 1.3;
		}
		else if (x <= 1)
		{
			x = (x - signalBreakP010) / (1.0 - signalBreakP010);
			x = std::asin(x);
			x = x * 0.3 / pi2 + 0.7;
			return x;
		}

		return 1;
	};

private:
	inline static bool _initialized = false;
	inline static std::array<uint8_t, 1024> lutP010_y;
	inline static std::array<uint8_t, 1024> lutP010_uv;

	FrameDecoderUtils()
	{
		if (std::exchange(_initialized, true))
			return;

		for (int i = 0; i < static_cast<int>(lutP010_y.size()); ++i)
		{
			constexpr int sourceRange = 1023;
			const double sourceValue = std::min(std::max(i, 0), sourceRange) / static_cast<double>(sourceRange);
			double val = packLuminanceP010(sourceValue);
			lutP010_y[i] = std::lround(val * 255.0);

			/*
			double unpack = unpackLuminanceP010(val);
			double delta = sourceValue - unpack;
			if (std::abs(delta) > 0.0000001)
			{
				bool error = true;
			}
			*/
		}


		for (int i = 0; i < static_cast<int>(lutP010_uv.size()); ++i)
		{
			constexpr int sourceRange = (960 - 64) / 2;
			const int current = std::abs(i - 512);
			const double sourceValue = std::min(current, sourceRange) / static_cast<double>(sourceRange);
			double val = packChromaP010(sourceValue);
			lutP010_uv[i] = std::max(std::min(128 + std::lround(((i < 512) ? -val : val) * 128.0), 255l), 0l);;

			/*
			double unpack = unpackChromaP010(val);
			double delta = sourceValue - unpack;
			if (std::abs(delta) > 0.0000001)
			{
				bool error = true;
			}
			*/
		}
	}
};
