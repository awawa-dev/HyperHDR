#pragma once

#include <effects/Animation_MoodBlobs.h>

#define ANIM_WARM_MOOD_BLOBS "Warm mood blobs"

class Animation_WarmMoodBlobs : public Animation_MoodBlobs
{
public:

	Animation_WarmMoodBlobs();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
