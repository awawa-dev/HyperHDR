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

#include <led-drivers/InfiniteColorEngineRgbw.h>
#include <infinite-color-engine/ColorSpace.h>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace linalg::aliases;

namespace
{
	inline uint16_t floatToQ16(const float value)
	{
		const float clamped = std::clamp(value, 0.0f, 1.0f);
		return static_cast<uint16_t>(std::lround(clamped * 65535.0f));
	}
}

float4 InfiniteColorEngineRgbw::computeRgbwTarget(const float3& rgbCalibrated, float whiteMixerThreshold, float whiteLedIntensity, const float3& whitePointRgb) const
{
	const float3 clampedRgb = linalg::clamp(rgbCalibrated, 0.0f, 1.0f);
	if (linalg::maxelem(clampedRgb) < 1e-5f)
		return float4(0.0f, 0.0f, 0.0f, 0.0f);

	constexpr float denom = 0.00001f;
	if (whiteLedIntensity > denom)
	{
		const float common = linalg::minelem(clampedRgb * whitePointRgb);
		const float mixerRange = 1.0f - whiteMixerThreshold;
		const float mixFactor = (mixerRange > denom) ? std::clamp((common - whiteMixerThreshold) / mixerRange, 0.0f, 1.0f) : 1.0f;
		const float baseWhiteAmount = common * mixFactor;
		const float3 rgbToSubtract = whitePointRgb * baseWhiteAmount;
		const float3 rgbTarget = linalg::clamp(clampedRgb - rgbToSubtract, 0.0f, 1.0f);
		const float whiteOutput = std::clamp(baseWhiteAmount / whiteLedIntensity, 0.0f, 1.0f);

		return float4(rgbTarget.x, rgbTarget.y, rgbTarget.z, whiteOutput);
	}

	return float4(clampedRgb.x, clampedRgb.y, clampedRgb.z, 0.0f);
}

void InfiniteColorEngineRgbw::renderRgbwFrame(const std::vector<float3>& infiniteColors, const int currentInterval, const float whiteMixerThreshold, const float whiteLedIntensity, const float3& whitePointRgb,
	std::vector<uint8_t>& output, size_t writeIndex, LedString::ColorOrder colorOrder)
{
	const size_t ledCount = infiniteColors.size();

	if (states.size() != ledCount)
		states.assign(ledCount, LEDState{});

	auto wanted = writeIndex + ledCount * sizeof(byte4);
	if (output.size() < wanted)
		output.resize(wanted, 0x00);

	auto colorIt = infiniteColors.cbegin();
	auto stateIt = states.begin();

	const float whiteMixerThreshold255 = whiteMixerThreshold * 255.0f;
	constexpr double motionThreshold = 0.01 / 255.0;
	const float actualMotionThreshold = (currentInterval > 0 && currentInterval < 17) ? static_cast<float>(motionThreshold * (currentInterval / 17.0)) : static_cast<float>(motionThreshold);
	const bool customWhite = linalg::minelem(whitePointRgb) < 0.99999f || linalg::maxelem(whitePointRgb) > 1.00001f;

	for (; colorIt != infiniteColors.cend(); ++colorIt, ++stateIt)
	{
		byte4 led = customWhite ? encodeRgbwFrame<true>(*colorIt, *stateIt, actualMotionThreshold, whiteMixerThreshold255, whiteLedIntensity, whitePointRgb, colorOrder)
			: encodeRgbwFrame<false>(*colorIt, *stateIt, actualMotionThreshold, whiteMixerThreshold255, whiteLedIntensity, whitePointRgb, colorOrder);
		std::memcpy(output.data() + writeIndex, &led, sizeof(led));
		writeIndex += sizeof(led);
	}
}

void InfiniteColorEngineRgbw::renderRgbwQ16Frame(const std::vector<float3>& infiniteColors, const float whiteMixerThreshold, const float whiteLedIntensity, const float3& whitePointRgb, std::vector<RgbwTargetQ16>& output)
{
	output.clear();
	output.reserve(infiniteColors.size());

	for (const float3& color : infiniteColors)
	{
		const float4 target = computeRgbwTarget(color, whiteMixerThreshold, whiteLedIntensity, whitePointRgb);
		output.push_back(RgbwTargetQ16{ floatToQ16(target.x), floatToQ16(target.y), floatToQ16(target.z), floatToQ16(target.w) });
	}
}

template<bool CustomWhiteTemp>
byte4 InfiniteColorEngineRgbw::encodeRgbwFrame(const float3& rgbCalibrated, LEDState& state, const float motionThreshold, const float whiteMixerThreshold, const float whiteLedIntensity, const float3& whitePointRgb, LedString::ColorOrder colorOrder)
{
	auto signFun = [](float x) noexcept { return (x > 0) - (x < 0); };

	if (state.last_input == rgbCalibrated)
	{
		state.error = float3{ 0.0f };
		return state.last_sent_bytes;
	}

	if (linalg::maxelem(rgbCalibrated) < 0.5f / 255.0f)
	{
		state = LEDState{};
		state.last_input = rgbCalibrated;
		return state.last_sent_bytes;
	}

	constexpr float denom = 0.00001f;
	const float3 diff = rgbCalibrated - state.last_input;
	const float3 diffAbs = linalg::abs(diff);
	const int diffMaxIndex = linalg::argmax(diffAbs);
	const int8_t newFlow = signFun(diff[diffMaxIndex]);
	const bool enableDithering = ((std::exchange(state.flow_control, newFlow) == newFlow) && diffAbs[diffMaxIndex] >= motionThreshold);

	const float3 input255 = enableDithering ? rgbCalibrated * 255.0f + state.error : rgbCalibrated * 255.0f;
	float4 target4;
	if (whiteLedIntensity > denom)
	{
		const float common = CustomWhiteTemp ? linalg::minelem(input255 * whitePointRgb) : linalg::minelem(input255);
		const float mixerDenom = 255.0f - whiteMixerThreshold;
		const float mixFactor = (mixerDenom > denom) ? std::clamp((common - whiteMixerThreshold) / mixerDenom, 0.0f, 1.0f) : 1.0f;
		const float baseWhiteAmount = common * mixFactor;
		const float whiteOutput = std::round(baseWhiteAmount / whiteLedIntensity);
		const float3 rgbSub = CustomWhiteTemp ? (whitePointRgb * whiteOutput * whiteLedIntensity) : float3{ whiteOutput * whiteLedIntensity };
		const float3 targetRgb = (whiteOutput != 0.0f) ? input255 - rgbSub : input255;
		target4 = float4(targetRgb.x, targetRgb.y, targetRgb.z, whiteOutput);
	}
	else
	{
		target4 = float4(input255.x, input255.y, input255.z, 0.0f);
	}

	const float4 targetRounded = linalg::clamp(linalg::round(target4), 0.0f, 255.0f);
	if (enableDithering)
	{
		const float deltaMotion = linalg::length2(diff);
		const float leak = linalg::lerp(0.96f, 0.80f, std::clamp(deltaMotion * 50.0f, 0.0f, 1.0f));
		state.error.x = (target4.x - targetRounded.x) * leak;
		state.error.y = (target4.y - targetRounded.y) * leak;
		state.error.z = (target4.z - targetRounded.z) * leak;
	}
	else
	{
		state.error = float3{ 0.0f };
	}

	switch (const auto base = static_cast<byte4>(targetRounded); colorOrder)
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
			break;
	}

	state.last_input = rgbCalibrated;
	return state.last_sent_bytes;
}