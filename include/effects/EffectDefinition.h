#pragma once

#ifndef PCH_ENABLED
	#include <string>
	#include <functional>
#endif

class AnimationBase;

typedef std::function<AnimationBase* ()> EffectConstructor;

struct EffectDefinition
{
	std::string	name;
	bool		smoothingDirectMode;
	bool		smoothingCustomSettings;	
	int			smoothingTime;
	int			smoothingFrequency;
	unsigned	smoothingConfig;
	EffectConstructor factory;

	EffectDefinition(EffectConstructor effectFactory, int _smoothingTime = 0, int _smoothingFrequency = 0) :
		EffectDefinition(false, effectFactory, _smoothingTime, _smoothingFrequency)
	{		
	}

	EffectDefinition(bool _directMode, EffectConstructor effectFactory, int _smoothingTime = 0, int _smoothingFrequency = 0) :
		smoothingDirectMode(_directMode),
		smoothingCustomSettings(_smoothingFrequency != 0 || _directMode),
		smoothingTime(_smoothingTime),
		smoothingFrequency(_smoothingFrequency),
		smoothingConfig(0),
		factory(effectFactory)
	{

	}
};
