#include <effectengine/Animation_WarmMoodBlobs.h>

Animation_WarmMoodBlobs::Animation_WarmMoodBlobs(QString name) :
	Animation_MoodBlobs(name)
{
	rotationTime = 60.0;
	color = { 255,0,0 };
	hueChange = 30.0;
	blobs = 5;
	reverse = false;
	baseColorChange = true;
	baseColorRangeLeft = 333;
	baseColorRangeRight = 151;
	baseColorChangeRate = 2.0;
}

EffectDefinition Animation_WarmMoodBlobs::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_WARM_MOOD_BLOBS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_WarmMoodBlobs::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = true;
	doc["smoothing-time_ms"] = 200;
	doc["smoothing-updateFrequency"] = 25.0;
	return doc;
}
