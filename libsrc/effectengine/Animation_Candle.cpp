#include <effectengine/Animation_Candle.h>

Animation_Candle::Animation_Candle(QString name) :
	Animation_CandleLight(name)
{
	colorShift = 4;	
	color = { 255, 138, 0 };	
}

EffectDefinition Animation_Candle::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_CANDLE;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_Candle::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = true;
	doc["smoothing-time_ms"] = 500;
	doc["smoothing-updateFrequency"] = 20.0;
	return doc;
}
