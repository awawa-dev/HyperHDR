#pragma once

#include <effectengine/Animation_MoodBlobs.h>

#define ANIM_COLD_MOOD_BLOBS "Cold mood blobs"

class Animation_ColdMoodBlobs : public Animation_MoodBlobs
{
	Q_OBJECT

private:

	static QJsonObject GetArgs();

public:

	Animation_ColdMoodBlobs(QString name = ANIM_COLD_MOOD_BLOBS);

	static EffectDefinition getDefinition();
};
