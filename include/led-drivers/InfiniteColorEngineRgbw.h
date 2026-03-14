#pragma once

#include <vector>
#include <linalg.h>

class InfiniteColorEngineRgbw
{
public:
	void renderRgbwFrame(const std::vector<linalg::aliases::float3>& infiniteColors, const float& whiteMixerThreshold, const float& whiteLedIntensity, const linalg::aliases::float3& whitePointRgb, std::vector<uint8_t>& output, size_t writeIndex, bool rgbOrder);

private:
	linalg::aliases::byte4 encodeRgbwFrame(const linalg::aliases::float3& rgbCalibrated, linalg::aliases::float4& oldRgbCalibrated, const float& whiteMixerThreshold, const float& whiteLedIntensity, const linalg::aliases::float3& whitePointRgb, bool rgbOrder);
	std::vector<linalg::aliases::float4> states;
};
