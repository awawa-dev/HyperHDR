#pragma once

#include <effectengine/Animation_MoodBlobs.h>

#define ANIM_GREEN_MOOD_BLOBS "Green mood blobs"

class Animation_GreenMoodBlobs : public Animation_MoodBlobs
{
public:

	Animation_GreenMoodBlobs(QString name = ANIM_GREEN_MOOD_BLOBS);

	static EffectDefinition getDefinition();
};
