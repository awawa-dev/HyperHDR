#pragma once

#include <vector>
#include <linalg.h>
#include <base/LedString.h>

class InfiniteColorEngineRgbw
{
	struct LEDState {
		linalg::aliases::float4 error = { 0.0f, 0.0f, 0.0f, 0.0f };
		linalg::aliases::float3 last_input = { 0.0f, 0.0f, 0.0f };
		linalg::aliases::float4 last_output = { 0.0f, 0.0f, 0.0f, 0.0f };
		linalg::aliases::byte4 last_sent_bytes = { 0, 0, 0, 0 };
		bool initialized = false;
	};

public:
	void renderRgbwFrame(const std::vector<linalg::aliases::float3>& infiniteColors, const float& whiteMixerThreshold, const float& whiteLedIntensity, const linalg::aliases::float3& whitePointRgb, std::vector<uint8_t>& output, size_t writeIndex, LedString::ColorOrder colorOrder);

private:	
	linalg::aliases::byte4 encodeRgbwFrame(const linalg::aliases::float3& rgbCalibrated, LEDState& state, const float& whiteMixerThreshold, const float& whiteLedIntensity, const linalg::aliases::float3& whitePointRgb, LedString::ColorOrder colorOrder);
	std::vector<LEDState> states;
};
