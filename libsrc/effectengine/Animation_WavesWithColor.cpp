#include <effectengine/Animation_WavesWithColor.h>

Animation_WavesWithColor::Animation_WavesWithColor(QString name) :
	Animation_Waves(name)
{	
	reverse = false;
}

EffectDefinition Animation_WavesWithColor::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_WAVESWITHCOLOR;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_WavesWithColor::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
