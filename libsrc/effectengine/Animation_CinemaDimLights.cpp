#include <effectengine/Animation_CinemaDimLights.h>

Animation_CinemaDimLights::Animation_CinemaDimLights(QString name) :
	Animation_Fade(name)
{
	colorStart = { 136, 97, 7 };
	colorEnd = { 238, 173, 47 };	
	fadeInTime = 0;
	fadeOutTime = 5000;
	colorStartTime = 0;
	colorEndTime = 0;
	repeat = 1;
	maintainEndCol = true;	
}

EffectDefinition Animation_CinemaDimLights::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_CINEMA_DIM_LIGHTS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_CinemaDimLights::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}
