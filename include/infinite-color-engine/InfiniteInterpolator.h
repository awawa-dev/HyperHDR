#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>

	#include <vector>
#endif

#include <infinite-color-engine/SharedOutputColors.h>
#include <linalg.h>

class InfiniteInterpolator {
protected:
	bool _isAnimationComplete = true;

public:
	virtual ~InfiniteInterpolator() = default;

	virtual void setTargetColors(std::vector<linalg::aliases::float3>&& new_rgb_targets, long long startTimeMs, bool debug) = 0;
	virtual void updateCurrentColors(long long currentTimeMs, float minBrightness) = 0;
	virtual SharedOutputColors getCurrentColors(float minBrightness) = 0;
	virtual void resetState() = 0;

	virtual void setTransitionDuration(float /*durationMs*/) {};
	virtual void setSpringiness(float /*stiffness*/, float /*damping*/) {};
	virtual void setMaxLuminanceChangePerFrame(float /*maxYChangePerFrame*/) {};
	virtual void setSmoothingFactor(float /*factor*/) {};
	bool isAnimationComplete() { return _isAnimationComplete; }
};
