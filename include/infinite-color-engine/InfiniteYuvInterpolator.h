#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>

	#include <vector>
#endif

#include <infinite-color-engine/InfiniteInterpolator.h>

class InfiniteYuvInterpolator : public InfiniteInterpolator
{
public:
	InfiniteYuvInterpolator();

	void setTargetColors(std::vector<linalg::aliases::float3>&& new_rgb_to_yuv_targets, float startTimeMs, bool debug = false) override;
	void updateCurrentColors(float currentTimeMs) override;
	SharedOutputColors getCurrentColors(float minBrightness = 0.f) override;

	void setTransitionDuration(float durationMs) override;
	void setMaxLuminanceChangePerFrame(float maxChangePerStep) override;
	void setSmoothingFactor(float factor) override;

	void resetToColors(std::vector<linalg::aliases::float3> colors, float startTimeMs);
	static void test();

private:
	std::vector<linalg::aliases::float3> _targetColorsRGB;
	std::optional<std::vector<linalg::aliases::float3>> _currentColorsRGB;
	std::vector<linalg::aliases::float3> _currentColorsYUV;
	std::vector<linalg::aliases::float3> _targetColorsYUV;

	float _initialDuration = 150.0f;
	float _startAnimationTimeMs = 0.0f;
	float _targetTime = 0.0f;
	float _lastUpdate = 0.0f;
	float _maxLuminanceChangePerStep = 0.02f;
	float _smoothingFactor = 0.0f;
};
