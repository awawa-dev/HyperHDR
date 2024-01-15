#pragma once

#include <effectengine/Animation_Fade.h>

#define ANIM_NOTIFY_BLUE "Notify blue"

class Animation_NotifyBlue : public Animation_Fade
{
public:

	Animation_NotifyBlue(QString name = ANIM_NOTIFY_BLUE);

	static EffectDefinition getDefinition();
};
