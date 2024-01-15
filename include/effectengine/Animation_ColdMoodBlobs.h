#pragma once

#include <effectengine/Animation_MoodBlobs.h>

#define ANIM_COLD_MOOD_BLOBS "Cold mood blobs"

class Animation_ColdMoodBlobs : public Animation_MoodBlobs
{
public:

	Animation_ColdMoodBlobs(QString name = ANIM_COLD_MOOD_BLOBS);

	static EffectDefinition getDefinition();
};
