#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>

	#include <vector>
#endif

#include <infinite-color-engine/InfiniteInterpolator.h>

class InfiniteRgbInterpolator : public InfiniteInterpolator
{
public:
	InfiniteRgbInterpolator();

	void setTargetColors(std::vector<linalg::aliases::float3> new_rgb_targets, float startTimeMs) override;
	void updateCurrentColors(float currentTimeMs) override;
	SharedOutputColors getCurrentColors() const override;

	void setTransitionDuration(float durationMs) override;
	void setSmoothingFactor(float factor) override;

	void resetToColors(std::vector<linalg::aliases::float3> colors);
	static void test();

private:
	std::vector<linalg::aliases::float3> _startColorsRGB;
	std::vector<linalg::aliases::float3> _targetColorsRGB;
	std::vector<linalg::aliases::float3> _currentColorsRGB;

	float _initialDistance = 0.0f;
	float _initialDuration = 300.0f;

	float _startTimeMs = 0.0f;
	float _deltaMs = 300.0f;
	float _smoothingFactor = 0.0f;
	bool _isAnimationComplete = true;

	static constexpr float FINISH_THRESHOLD = 1e-4f;
	static constexpr float FINISH_THRESHOLD_SQ = FINISH_THRESHOLD * FINISH_THRESHOLD;
};
