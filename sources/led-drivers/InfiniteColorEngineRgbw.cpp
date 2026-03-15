/* InfiniteColorEngineRgbw.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
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
#include <QByteArray>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <linalg.h>
#include <limits>
#include <led-drivers/InfiniteColorEngineRgbw.h>
#include <infinite-color-engine/ColorSpace.h>
using namespace linalg::aliases;

void InfiniteColorEngineRgbw::renderRgbwFrame(const std::vector<float3>& infiniteColors, const float& whiteMixerThreshold, const float& whiteLedIntensity, const float3& whitePointRgb,
											std::vector<uint8_t>& output, size_t writeIndex, bool rgbOrder)
{
	const size_t ledCount = infiniteColors.size();

	if (states.size() != ledCount)
	{
		states.clear();
		states.resize(ledCount, float4{ 0.0f});
	}

	auto wanted = writeIndex + ledCount * sizeof(byte4);
	if (output.size() < wanted)
		output.resize(wanted, 0x00);

	auto colorIt = infiniteColors.cbegin();
	auto stateIt = states.begin();

	for (; colorIt != infiniteColors.cend(); ++colorIt, ++stateIt)
	{
		byte4 led = encodeRgbwFrame(*colorIt, *stateIt, whiteMixerThreshold, whiteLedIntensity, whitePointRgb, rgbOrder);
		std::memcpy(output.data() + writeIndex, &led, sizeof(led));
		writeIndex += sizeof(led);
	}
}

byte4 InfiniteColorEngineRgbw::encodeRgbwFrame(const float3& rgbCalibrated, float4& oldRgbCalibrated, const float& whiteMixerThreshold, const float& whiteLedIntensity, const float3& whitePointRgb, bool rgbOrder)
{
	constexpr float denom = 0.00001f;
	float4 target4;
	if (whiteLedIntensity > denom)
	{
		float common = linalg::minelem(rgbCalibrated * whitePointRgb);
		float w_factor = std::clamp((common - whiteMixerThreshold) / (1.0f - whiteMixerThreshold), 0.0f, 1.0f);
		float base_w_amount = common * w_factor;
		float3 rgb_to_subtract = whitePointRgb * base_w_amount;
		float3 rgbTarget = linalg::clamp(rgbCalibrated - rgb_to_subtract, 0.0f, 1.0f);
		float w_output = base_w_amount / whiteLedIntensity;

		target4 = float4(rgbTarget.x, rgbTarget.y, rgbTarget.z, w_output);
	}
	else {
		target4 = float4(rgbCalibrated.x, rgbCalibrated.y, rgbCalibrated.z, 0);
	}

	if (linalg::maxelem(linalg::abs(target4 - oldRgbCalibrated) * 255.0f) > 0.49f)
	{
		oldRgbCalibrated = target4;
	}

	auto finalColor = ColorSpaceMath::round_to_0_255<linalg::aliases::byte4>(oldRgbCalibrated * 255.0f);

	return (rgbOrder) ? static_cast<byte4>(finalColor) : byte4(
		static_cast<uint8_t>(finalColor.y), // G
		static_cast<uint8_t>(finalColor.x), // R
		static_cast<uint8_t>(finalColor.z), // B
		static_cast<uint8_t>(finalColor.w)  // W
	);
}
