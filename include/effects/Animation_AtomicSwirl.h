#pragma once

#include <effects/Animation_Swirl.h>

#define ANIM_ATOMIC_SWIRL "Atomic swirl"

class Animation_AtomicSwirl : public Animation_Swirl
{
public:

	Animation_AtomicSwirl();

	static EffectDefinition getDefinition();

private:
	static bool isRegistered;

};
