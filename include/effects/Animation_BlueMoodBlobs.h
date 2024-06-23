#pragma once

#include <effects/Animation_MoodBlobs.h>

#define ANIM_BLUE_MOOD_BLOBS "Blue mood blobs"

class Animation_BlueMoodBlobs : public Animation_MoodBlobs
{
public:

	Animation_BlueMoodBlobs();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
