#pragma once

#include <vector>
#include <linalg.h>
#include <base/LedString.h>

class InfiniteColorEngineRgbw
{
	struct LEDState {
		linalg::aliases::float3 error = linalg::aliases::float3{ 0.0f };
		linalg::aliases::float3 last_input = linalg::aliases::float3{ -1.0f };
		linalg::aliases::byte4 last_sent_bytes = linalg::aliases::byte4{ uint8_t(0) };
		int8_t flow_control = 0;
	};

public:
	void renderRgbwFrame(const std::vector<linalg::aliases::float3>& infiniteColors, const int currentInterval, const float whiteMixerThreshold, const float whiteLedIntensity, const linalg::aliases::float3& whitePointRgb, std::vector<uint8_t>& output, size_t writeIndex, LedString::ColorOrder colorOrder);

private:
	template<bool CustomWhiteTemp>
	linalg::aliases::byte4 encodeRgbwFrame(const linalg::aliases::float3& rgbCalibrated, LEDState& state, const float motionThreshold, const float whiteMixerThreshold, const float whiteLedIntensity, const linalg::aliases::float3& whitePointRgb, LedString::ColorOrder colorOrder);

	std::vector<LEDState> states;
};
