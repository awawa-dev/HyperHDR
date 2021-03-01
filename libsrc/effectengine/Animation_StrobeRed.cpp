#include <effectengine/Animation_StrobeRed.h>

Animation_StrobeRed::Animation_StrobeRed(QString name) :
	Animation_Fade(name)
{
	colorStart = { 255, 0, 0 };
	colorEnd = {  0, 0, 0 };	
	fadeInTime = 125;
	fadeOutTime = 125;
	colorStartTime = 125;
	colorEndTime = 125;
	repeat = 0;
	maintainEndCol = true;	
}

EffectDefinition Animation_StrobeRed::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_STROBE_RED;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_StrobeRed::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}
