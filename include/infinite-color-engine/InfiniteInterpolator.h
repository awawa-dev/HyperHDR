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
public:
	virtual ~InfiniteInterpolator() = default;

	virtual void setTargetColors(std::vector<linalg::aliases::float3>&& new_rgb_targets, float startTimeMs) = 0;
	virtual void updateCurrentColors(float currentTimeMs) = 0;
	virtual SharedOutputColors getCurrentColors() const = 0;

	virtual void setTransitionDuration(float /*durationMs*/) {};
	virtual void setSpringiness(float /*stiffness*/, float /*damping*/) {};
	virtual void setMaxLuminanceChangePerFrame(float /*maxYChangePerFrame*/) {};
	virtual void setSmoothingFactor(float /*factor*/) {};

};
