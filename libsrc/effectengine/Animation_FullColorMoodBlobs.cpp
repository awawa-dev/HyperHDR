#include <effectengine/Animation_FullColorMoodBlobs.h>

Animation_FullColorMoodBlobs::Animation_FullColorMoodBlobs(QString name) :
	Animation_MoodBlobs(name)
{
	rotationTime = 60.0;
	colorRandom = true;
	hueChange = 30.0;
	blobs = 5;
	reverse = false;
	baseColorChange = true;
	baseColorRangeLeft = 0;
	baseColorRangeRight = 360;
	baseColorChangeRate = 0.2;
}

EffectDefinition Animation_FullColorMoodBlobs::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_FULLCOLOR_MOOD_BLOBS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_FullColorMoodBlobs::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
