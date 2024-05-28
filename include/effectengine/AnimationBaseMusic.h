#pragma once

#include <effectengine/AnimationBase.h>
#include <base/SoundCaptureResult.h>

class SoundCapture;

class AnimationBaseMusic : public AnimationBase
{
public:
	AnimationBaseMusic(QString name);
	~AnimationBaseMusic();

	bool isSoundEffect() override;

	void store(MovingTarget* source);
	void restore(MovingTarget* target);

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

protected:
	std::shared_ptr<SoundCapture> _soundCapture;

private:
	MovingTarget _myTarget;
	uint32_t	_soundHandle;
};

