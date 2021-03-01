#pragma once

#include <effectengine/Animation_Swirl.h>

#define ANIM_DOUBLE_SWIRL "Double swirl"

class Animation_DoubleSwirl : public Animation_Swirl
{
	Q_OBJECT

	static QJsonObject GetArgs();

public:
	Animation_DoubleSwirl(QString name=ANIM_DOUBLE_SWIRL);

	static EffectDefinition getDefinition();	
};
