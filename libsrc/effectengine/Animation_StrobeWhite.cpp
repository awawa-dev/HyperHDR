#include <effectengine/Animation_StrobeWhite.h>

Animation_StrobeWhite::Animation_StrobeWhite(QString name) :
	Animation_Fade(name)
{
	colorStart = { 0, 0, 0 };
	colorEnd = {   255, 255, 255 };	
	fadeInTime = 200;
	fadeOutTime = 0;
	colorStartTime = 0;
	colorEndTime = 300;
	repeat = 0;
	maintainEndCol = true;
}

EffectDefinition Animation_StrobeWhite::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_STROBE_WHITE;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_StrobeWhite::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}
