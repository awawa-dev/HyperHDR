#pragma once

#include <effects/Animation_MoodBlobs.h>

#define ANIM_COLD_MOOD_BLOBS "Cold mood blobs"

class Animation_ColdMoodBlobs : public Animation_MoodBlobs
{
public:

	Animation_ColdMoodBlobs();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
