#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>

	#include <vector>
#endif

#include <infinite-color-engine/InfiniteInterpolator.h>

class InfiniteHybridInterpolator : public InfiniteInterpolator
{
public:
	InfiniteHybridInterpolator();

	void setTargetColors(std::vector<linalg::aliases::float3>&& new_rgb_targets, float startTimeMs, bool debug = false) override;
	void updateCurrentColors(float currentTimeMs) override;
	SharedOutputColors getCurrentColors() const override;

	void setTransitionDuration(float durationMs) override;
	void setSpringiness(float stiffness, float damping) override;
	void setMaxLuminanceChangePerFrame(float maxYChangePerFrame) override;

	void resetToColors(std::vector<linalg::aliases::float3> colors);
	static void test();

private:
	std::vector<linalg::aliases::float3> _currentColorsRGB;
	std::vector<linalg::aliases::float3> _currentColorsYUV;
	std::vector<linalg::aliases::float3> _velocityYUV;

	std::vector<linalg::aliases::float3> _pacer_startColorsYUV;
	std::vector<linalg::aliases::float3> _pacer_targetColorsYUV;
	std::vector<linalg::aliases::float3> _finalTargetRGB;

	float _startTimeMs = 0.0f;
	float _lastTimeMs = 0.0f;
	float _deltaMs = 300.0f;
	float _initialDistance = 0.0f;
	float _initialDuration = 300.0f;

	float _stiffness = 200.0f;
	float _damping = 26.0f;
	float _maxLuminanceChangePerFrame = 0;

	bool _isAnimationComplete = true;

	static constexpr float FINISH_THRESHOLD = 1e-4f;
	static constexpr float FINISH_THRESHOLD_SQ = FINISH_THRESHOLD * FINISH_THRESHOLD;
};
