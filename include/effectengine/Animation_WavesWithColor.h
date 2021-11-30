#pragma once

#include <effectengine/Animation_Waves.h>

#define ANIM_WAVESWITHCOLOR "Waves with Color"

class Animation_WavesWithColor : public Animation_Waves
{
	Q_OBJECT

private:

	static QJsonObject GetArgs();

public:
	Animation_WavesWithColor(QString name = ANIM_WAVESWITHCOLOR);

	static EffectDefinition getDefinition();
};
