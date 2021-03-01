#pragma once

#include <effectengine/Animation_MoodBlobs.h>

#define ANIM_BLUE_MOOD_BLOBS "Blue mood blobs"

class Animation_BlueMoodBlobs : public Animation_MoodBlobs
{
	Q_OBJECT

	static QJsonObject GetArgs();

public:
	Animation_BlueMoodBlobs(QString name=ANIM_BLUE_MOOD_BLOBS);

	static EffectDefinition getDefinition();
};
