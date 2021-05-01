#pragma once

#include <effectengine/Animation_CandleLight.h>

#define ANIM_CANDLE "Candle"

class Animation_Candle : public Animation_CandleLight
{
	Q_OBJECT

	static QJsonObject GetArgs();

public:
	Animation_Candle(QString name= ANIM_CANDLE);

	static EffectDefinition getDefinition();
};
