#pragma once

#include <effectengine/Animation_Waves.h>

#define ANIM_SEAWAVES "Sea waves"

class Animation_SeaWaves : public Animation_Waves
{
	Q_OBJECT

private:

	static QJsonObject GetArgs();

public:

	Animation_SeaWaves(QString name = ANIM_SEAWAVES);

	static EffectDefinition getDefinition();
};
