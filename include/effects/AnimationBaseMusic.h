#pragma once

#ifndef PCH_ENABLED
	#include <functional>
#endif

#include <effects/AnimationBase.h>
#include <base/SoundCaptureResult.h>

class SoundCapture;

class AnimationBaseMusic : public AnimationBase
{
public:
	AnimationBaseMusic();
	~AnimationBaseMusic();

	bool isSoundEffect() override;

	void store(MovingTarget* source);
	void restore(MovingTarget* target);

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	std::function<SoundCaptureResult* (AnimationBaseMusic* effect, uint32_t& lastIndex, bool* newAverage, bool* newSlow, bool* newFast, int* isMulti)> hasResult;
private:
	MovingTarget _myTarget;
};

