#include <effectengine/Animation_PoliceLightsSingle.h>

Animation_PoliceLightsSingle::Animation_PoliceLightsSingle(QString name) :
	Animation_Police(name)
{
	rotationTime = 1.5;
	colorOne = {255, 0, 0};
	colorTwo = { 0, 0, 255 };
	colorsCount = 10;
	reverse = false;
}

EffectDefinition Animation_PoliceLightsSingle::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_POLICELIGHTSSINGLE;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_PoliceLightsSingle::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}
