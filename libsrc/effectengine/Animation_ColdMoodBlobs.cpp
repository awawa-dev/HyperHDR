#include <effectengine/Animation_ColdMoodBlobs.h>

Animation_ColdMoodBlobs::Animation_ColdMoodBlobs(QString name) :
	Animation_MoodBlobs(name)
{
	rotationTime = 60.0;
	color = { 0,0,255 };
	hueChange = 30.0;
	blobs = 5;
	reverse = false;
	baseColorChange = true;
	baseColorRangeLeft = 160;
	baseColorRangeRight = 320;
	baseColorChangeRate = 2.0;
}

EffectDefinition Animation_ColdMoodBlobs::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_COLD_MOOD_BLOBS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_ColdMoodBlobs::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
