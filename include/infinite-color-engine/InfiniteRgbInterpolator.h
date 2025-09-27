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

	void setTargetColors(std::vector<linalg::aliases::float3>&& new_rgb_targets, float startTimeMs, bool debug = false) override;
	void updateCurrentColors(float currentTimeMs) override;
	SharedOutputColors getCurrentColors() override;

	void setTransitionDuration(float durationMs) override;
	void setSmoothingFactor(float factor) override;

	void resetToColors(std::vector<linalg::aliases::float3> colors);
	static void test();

private:
	std::vector<linalg::aliases::float3> _currentColorsRGB;
	std::vector<linalg::aliases::float3> _targetColorsRGB;

	float _smoothingFactor = 0.0f;
	float _initialDuration = 150.0f;
	float _startAnimationTimeMs = 0.0f;
	float _targetTime = 0.0f;
	float _lastUpdate = 0.0f;
	bool _isAnimationComplete = true;
};
