#pragma once

#include <effectengine/Animation_Fade.h>

#define ANIM_STROBE_RED "Strobe red"

class Animation_StrobeRed : public Animation_Fade
{
	Q_OBJECT

	static QJsonObject GetArgs();

public:
	Animation_StrobeRed(QString name=ANIM_STROBE_RED);

	static EffectDefinition getDefinition();
};
