#pragma once

#include <effects/Animation_CandleLight.h>

#define ANIM_CANDLE "Candle"

class Animation_Candle : public Animation_CandleLight
{
public:
	Animation_Candle();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
