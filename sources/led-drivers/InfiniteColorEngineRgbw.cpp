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

void InfiniteColorEngineRgbw::renderRgbwFrame(const std::vector<float3>& infiniteColors, const int currentInterval, const float whiteMixerThreshold, const float whiteLedIntensity, const float3& whitePointRgb,
											std::vector<uint8_t>& output, size_t writeIndex, LedString::ColorOrder colorOrder)
{
	const size_t ledCount = infiniteColors.size();

	if (states.size() != ledCount)
	{
		states.assign(ledCount, LEDState{});
	}

	auto wanted = writeIndex + ledCount * sizeof(byte4);
	if (output.size() < wanted)
		output.resize(wanted, 0x00);

	auto colorIt = infiniteColors.cbegin();
	auto stateIt = states.begin();

	const float whiteMixerThreshold255 = whiteMixerThreshold * 255.0f;
	constexpr double motionThreshold = 0.01 / 255.0;

	float actualMotionThreshold = (currentInterval > 0 && currentInterval < 17) ? motionThreshold * (currentInterval / 17.0): motionThreshold;
	bool customWhite = linalg::minelem(whitePointRgb) < 0.99999f || linalg::maxelem(whitePointRgb) > 1.00001f;

	for (; colorIt != infiniteColors.cend(); ++colorIt, ++stateIt)
	{
		byte4 led = (customWhite) ? encodeRgbwFrame<true>(*colorIt, *stateIt, actualMotionThreshold, whiteMixerThreshold255, whiteLedIntensity, whitePointRgb, colorOrder):
									encodeRgbwFrame<false>(*colorIt, *stateIt, actualMotionThreshold, whiteMixerThreshold255, whiteLedIntensity, whitePointRgb, colorOrder);
		std::memcpy(output.data() + writeIndex, &led, sizeof(led));
		writeIndex += sizeof(led);
	}
}

template<bool CustomWhiteTemp>
byte4 InfiniteColorEngineRgbw::encodeRgbwFrame(const float3& rgbCalibrated, LEDState& state, const float motionThreshold, const float whiteMixerThreshold, const float whiteLedIntensity, const float3& whitePointRgb, LedString::ColorOrder colorOrder)
{
	auto signFun = [](float x) noexcept {
		return (x > 0) - (x < 0);
	};

	if (state.last_input == rgbCalibrated) {
		state.error = float3{ 0.0f };
		return state.last_sent_bytes;
	}

	if (linalg::maxelem(rgbCalibrated) < 0.5f / 255.0f) {
		state = LEDState{};
		state.last_input = rgbCalibrated;
		return state.last_sent_bytes;
	}

	constexpr float denom = 0.00001f;

	const float3 diff = rgbCalibrated - state.last_input;
	const float3 diffAbs = linalg::abs(diff);
	const int    diffMaxIndex = linalg::argmax(diffAbs);
	const int8_t newFlow = signFun(diff[diffMaxIndex]);	
	bool enableDithering = ((std::exchange(state.flow_control, newFlow) == newFlow) && diffAbs[diffMaxIndex] >= motionThreshold);

	const float3 input255 = (enableDithering) ? rgbCalibrated * 255.0f + state.error : rgbCalibrated * 255.0f;
	float4 target4;
	if (whiteLedIntensity > denom)
	{
		const float common = (CustomWhiteTemp) ? linalg::minelem(input255 * whitePointRgb) : linalg::minelem(input255);
		const float w_mian = (255.0f - whiteMixerThreshold);
		const float w_factor = (w_mian > denom) ? std::clamp((common - whiteMixerThreshold) / w_mian, 0.0f, 1.0f) : 1.0f;
		const float base_w_amount = common * w_factor;
		const float w_output = std::round(base_w_amount / whiteLedIntensity);
		const float3 rgbSub = (CustomWhiteTemp) ? (whitePointRgb * w_output * whiteLedIntensity) : float3{ w_output * whiteLedIntensity };
		const float3 targetRgb = (w_output != 0.0f) ? input255 - rgbSub : input255;
		target4 = float4(targetRgb.x, targetRgb.y, targetRgb.z, w_output);
	}
	else {
		target4 = float4(input255.x, input255.y, input255.z, 0.0f);
	}

	const float4 target_rounded = linalg::clamp(linalg::round(target4), 0.0f, 255.0f);

	if (enableDithering){
		const float delta_motion = linalg::length2(diff);
		const float leak = linalg::lerp(0.96f, 0.80f, std::clamp(delta_motion * 50.0f, 0.0f, 1.0f));
		state.error.x = (target4.x - target_rounded.x) * leak;
		state.error.y = (target4.y - target_rounded.y) * leak;
		state.error.z = (target4.z - target_rounded.z) * leak;
	}
	else {
		state.error = float3{ 0.0f };
	}
	
	switch (const auto base = static_cast<byte4>(target_rounded);  colorOrder)
	{
		case LedString::ColorOrder::ORDER_BGR:
			state.last_sent_bytes = { base.z, base.y, base.x, base.w };
			break;
		case LedString::ColorOrder::ORDER_RBG:
			state.last_sent_bytes = { base.x, base.z, base.y, base.w };
			break;
		case LedString::ColorOrder::ORDER_GRB:
			state.last_sent_bytes = { base.y, base.x, base.z, base.w };
			break;
		case LedString::ColorOrder::ORDER_GBR:
			state.last_sent_bytes = { base.y, base.z, base.x, base.w };
			break;
		case LedString::ColorOrder::ORDER_BRG:
			state.last_sent_bytes = { base.z, base.x, base.y, base.w };
			break;
		default: [[likely]]
			state.last_sent_bytes = base;
	};

	state.last_input = rgbCalibrated;

	return state.last_sent_bytes;
}
