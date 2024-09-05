#pragma once

#include <effects/Animation_Fade.h>

#define ANIM_NOTIFY_BLUE "Notify blue"

class Animation_NotifyBlue : public Animation_Fade
{
public:

	Animation_NotifyBlue();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
