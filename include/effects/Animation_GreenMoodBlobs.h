#pragma once

#include <effects/Animation_MoodBlobs.h>

#define ANIM_GREEN_MOOD_BLOBS "Green mood blobs"

class Animation_GreenMoodBlobs : public Animation_MoodBlobs
{
public:

	Animation_GreenMoodBlobs();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
