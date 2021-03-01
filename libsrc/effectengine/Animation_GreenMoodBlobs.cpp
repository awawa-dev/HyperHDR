#include <effectengine/Animation_GreenMoodBlobs.h>

Animation_GreenMoodBlobs::Animation_GreenMoodBlobs(QString name) :
	Animation_MoodBlobs(name)
{
	rotationTime = 60.0;
	color = { 0,255,0 };
	hueChange = 60.0;
	blobs = 5;
	reverse = false;	
}

EffectDefinition Animation_GreenMoodBlobs::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_GREEN_MOOD_BLOBS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_GreenMoodBlobs::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
