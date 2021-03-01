#include <effectengine/Animation_CinemaBrightenLights.h>

Animation_CinemaBrightenLights::Animation_CinemaBrightenLights(QString name) :
	Animation_Fade(name)
{
	colorStart = { 136, 97, 7 };
	colorEnd = { 238, 173, 47 };	
	fadeInTime = 5000;
	fadeOutTime = 0;
	colorStartTime = 0;
	colorEndTime = 0;
	repeat = 1;
	maintainEndCol = true;	
}

EffectDefinition Animation_CinemaBrightenLights::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_CINEMA_BRIGHTEN_LIGHTS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_CinemaBrightenLights::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}
