#pragma once

#include <effectengine/Animation_MoodBlobs.h>

#define ANIM_FULLCOLOR_MOOD_BLOBS "Full color mood blobs"

class Animation_FullColorMoodBlobs : public Animation_MoodBlobs
{
public:
	Animation_FullColorMoodBlobs(QString name = ANIM_FULLCOLOR_MOOD_BLOBS);

	static EffectDefinition getDefinition();
};
