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

	void setTargetColors(std::vector<linalg::aliases::float3>&& new_rgb_targets, float startTimeMs, bool debug = false) override;
	void updateCurrentColors(float currentTimeMs) override;
	SharedOutputColors getCurrentColors() const override;

	void setTransitionDuration(float durationMs) override;
	void setMaxLuminanceChangePerFrame(float maxChangePerStep) override;

	void resetToColors(std::vector<linalg::aliases::float3> colors);
	static void test();

private:
	std::vector<linalg::aliases::float3> _targetColorsRGB;

	std::vector<linalg::aliases::float3> _startColorsYUV;
	std::vector<linalg::aliases::float3> _targetColorsYUV;

	std::vector<linalg::aliases::float3> _currentColorsRGB;
	std::vector<linalg::aliases::float3> _currentColorsYUV;

	std::vector<bool> _wasClamped; // Flaga do śledzenia spowolnionych animacji

	float _initialDistance = 0.0f;
	float _initialDuration = 300.0f;

	float _startTimeMs = 0.0f;
	float _deltaMs = 300.0f;
	float _maxLuminanceChangePerStep = 0.02f;
	bool _isAnimationComplete = true;

	static constexpr float YUV_FINISH_THRESHOLD = 2.5f / 255.0f;
};
