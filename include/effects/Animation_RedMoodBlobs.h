#pragma once

#include <effects/Animation_MoodBlobs.h>

#define ANIM_RED_MOOD_BLOBS "Red mood blobs"

class Animation_RedMoodBlobs : public Animation_MoodBlobs
{
public:
	Animation_RedMoodBlobs();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;
};
