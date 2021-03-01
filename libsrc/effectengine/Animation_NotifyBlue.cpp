#include <effectengine/Animation_NotifyBlue.h>

Animation_NotifyBlue::Animation_NotifyBlue(QString name) :
	Animation_Fade(name)
{
	colorStart = { 0, 0, 50 };
	colorEnd = { 0, 0, 255 };	
	fadeInTime = 200;
	fadeOutTime = 100;
	colorStartTime = 40;
	colorEndTime = 150;
	repeat = 3;
	maintainEndCol = false;	
}

EffectDefinition Animation_NotifyBlue::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_NOTIFY_BLUE;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_NotifyBlue::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}
