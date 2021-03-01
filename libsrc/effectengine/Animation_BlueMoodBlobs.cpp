#include <effectengine/Animation_BlueMoodBlobs.h>

Animation_BlueMoodBlobs::Animation_BlueMoodBlobs(QString name) :
	Animation_MoodBlobs(name)
{
	rotationTime = 60.0;
	color = { 0,0,255 };
	hueChange = 60.0;
	blobs = 5;
	reverse = false;	
}

EffectDefinition Animation_BlueMoodBlobs::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_BLUE_MOOD_BLOBS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_BlueMoodBlobs::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
