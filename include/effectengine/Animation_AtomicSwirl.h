#pragma once

#include <effectengine/Animation_Swirl.h>

#define ANIM_ATOMIC_SWIRL "Atomic swirl"

class Animation_AtomicSwirl : public Animation_Swirl
{
public:

	Animation_AtomicSwirl(QString name = ANIM_ATOMIC_SWIRL);

	static EffectDefinition getDefinition();
};
