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
											std::vector<uint8_t>& output, size_t writeIndex, LedString::ColorOrder colorOrder)
{
	const size_t ledCount = infiniteColors.size();

	if (states.size() != ledCount)
	{
		states.clear();
		states.reserve(ledCount);

		for (const auto& color : infiniteColors)
		{
			LEDState state{};
			state.last_input = color;
			state.initialized = false;
			states.emplace_back(state);
		}
	}

	auto wanted = writeIndex + ledCount * sizeof(byte4);
	if (output.size() < wanted)
		output.resize(wanted, 0x00);

	auto colorIt = infiniteColors.cbegin();
	auto stateIt = states.begin();

	for (; colorIt != infiniteColors.cend(); ++colorIt, ++stateIt)
	{
		byte4 led = encodeRgbwFrame(*colorIt, *stateIt, whiteMixerThreshold, whiteLedIntensity, whitePointRgb, colorOrder);
		std::memcpy(output.data() + writeIndex, &led, sizeof(led));
		writeIndex += sizeof(led);
	}
}

byte4 InfiniteColorEngineRgbw::encodeRgbwFrame(const float3& rgbCalibrated, LEDState& state, const float& whiteMixerThreshold, const float& whiteLedIntensity, const float3& whitePointRgb, LedString::ColorOrder colorOrder)
{
	if (state.last_input == rgbCalibrated && state.initialized) {
		return state.last_sent_bytes;
	}

	float max_in = linalg::maxelem(rgbCalibrated);
	if (max_in < 1e-5f) {
		state.last_sent_bytes = { 0, 0, 0, 0 };
		state.error = { 0.0f, 0.0f, 0.0f, 0.0f };
		state.last_input = rgbCalibrated;
		state.last_output = { 0.0f, 0.0f, 0.0f, 0.0f };
		state.initialized = true;
		return state.last_sent_bytes;
	}	

	constexpr float denom = 0.00001f;
	float4 target4;
	if (whiteLedIntensity > denom)
	{
		float common = linalg::minelem(rgbCalibrated * whitePointRgb);
		float w_mian = (1.0f - whiteMixerThreshold);
		float w_factor = (w_mian > denom) ? std::clamp((common - whiteMixerThreshold) / w_mian, 0.0f, 1.0f) : 1.0f;
		float base_w_amount = common * w_factor;
		float3 rgb_to_subtract = whitePointRgb * base_w_amount;
		float3 rgbTarget = linalg::clamp(rgbCalibrated - rgb_to_subtract, 0.0f, 1.0f);
		float w_output = base_w_amount / whiteLedIntensity;

		target4 = float4(rgbTarget.x, rgbTarget.y, rgbTarget.z, w_output);
	}
	else {
		target4 = float4(rgbCalibrated.x, rgbCalibrated.y, rgbCalibrated.z, 0);
	}

	float4 current_target = linalg::clamp(target4 + state.error, 0.0f, 1.0f);
	float4 potential_float = linalg::round(current_target * 255.0f) / 255.0f;
	float4 diff = linalg::abs(potential_float - state.last_output) * 255.0f;

	if (linalg::maxelem(diff) > 0.49f) {
		state.last_output = potential_float;
	}

	int4 final_out = static_cast<int4>(linalg::round(state.last_output * 255.0f));
	final_out = linalg::clamp(final_out, int4(0), int4(255));

	float delta_motion = linalg::length2(rgbCalibrated - state.last_input);
	float leak = linalg::lerp(0.96f, 0.80f, std::clamp(delta_motion * 50.0f, 0.0f, 1.0f));

	state.error = (current_target - state.last_output) * leak;

	switch (const auto base = static_cast<byte4>(final_out);  colorOrder)
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
	state.initialized = true;

	return state.last_sent_bytes;
}
