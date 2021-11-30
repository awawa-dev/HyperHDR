#pragma once

#include <effectengine/Animation_MoodBlobs.h>

#define ANIM_RED_MOOD_BLOBS "Red mood blobs"

class Animation_RedMoodBlobs : public Animation_MoodBlobs
{
	Q_OBJECT

private:

	static QJsonObject GetArgs();

public:
	Animation_RedMoodBlobs(QString name = ANIM_RED_MOOD_BLOBS);

	static EffectDefinition getDefinition();
};
