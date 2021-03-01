#include <effectengine/Animation_RedMoodBlobs.h>

Animation_RedMoodBlobs::Animation_RedMoodBlobs(QString name) :
	Animation_MoodBlobs(name)
{
	rotationTime = 60.0;
	color = { 255,0,0 };
	hueChange = 60.0;
	blobs = 5;
	reverse = false;
}

EffectDefinition Animation_RedMoodBlobs::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_RED_MOOD_BLOBS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_RedMoodBlobs::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
