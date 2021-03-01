#include <effectengine/Animation_PoliceLightsSolid.h>

Animation_PoliceLightsSolid::Animation_PoliceLightsSolid(QString name) :
	Animation_Police(name)
{
	rotationTime = 1.0;
	colorOne = {255, 0, 0};
	colorTwo = { 0, 0, 255 };
	reverse = false;
}

EffectDefinition Animation_PoliceLightsSolid::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_POLICELIGHTSSOLID;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_PoliceLightsSolid::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}
