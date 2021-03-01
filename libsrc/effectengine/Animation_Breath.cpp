#include <effectengine/Animation_Breath.h>

Animation_Breath::Animation_Breath(QString name) :
	Animation_Fade(name)
{
	colorEnd = { 255, 255, 255 };
	colorStart = { 50, 50, 50 };
	fadeInTime = 3000;
	fadeOutTime = 1000;
	colorStartTime = 50;
	colorEndTime = 250;
	repeat = 0;
	maintainEndCol = true;
}

EffectDefinition Animation_Breath::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_BREATH;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_Breath::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}
