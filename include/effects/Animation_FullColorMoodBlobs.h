#pragma once

#include <effects/Animation_MoodBlobs.h>

#define ANIM_FULLCOLOR_MOOD_BLOBS "Full color mood blobs"

class Animation_FullColorMoodBlobs : public Animation_MoodBlobs
{
public:
	Animation_FullColorMoodBlobs();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
