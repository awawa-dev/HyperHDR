#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>

	#include <vector>
#endif

#include <infinite-color-engine/InfiniteInterpolator.h>

class InfiniteHybridRgbInterpolator : public InfiniteInterpolator
{
public:
	InfiniteHybridRgbInterpolator();

	void setTargetColors(std::vector<linalg::aliases::float3>&& new_rgb_targets, float startTimeMs, bool debug = false) override;
	void updateCurrentColors(float currentTimeMs) override;
	SharedOutputColors getCurrentColors() override;

	void setTransitionDuration(float durationMs) override;
	void setSpringiness(float stiffness, float damping) override;
	void setSmoothingFactor(float factor) override;

	void resetToColors(std::vector<linalg::aliases::float3> colors, float startTimeMs);
	static void test();

private:
	std::vector<linalg::aliases::float3> _targetColorsRGB;
	std::vector<linalg::aliases::float3> _currentColorsRGB;
	std::vector<linalg::aliases::float3> _velocitiesRGB;

	float _initialDuration = 150.0f;
	float _startAnimationTimeMs = 0.0f;
	float _targetTime = 0.0f;
	float _lastUpdate = 0.0f;
	float _stiffness = 150.0f;
	float _damping = 26.0f;
	float _smoothingFactor = 0.0f;
};
