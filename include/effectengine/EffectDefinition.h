#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QJsonObject>
#endif

class AnimationBase;

struct EffectDefinition
{
	QString		name;
	bool		smoothingDirectMode;
	bool		smoothingCustomSettings;	
	int			smoothingTime;
	int			smoothingFrequency;
	unsigned	smoothingConfig;
	AnimationBase* (*factory)();

	EffectDefinition(AnimationBase* (*effectFactory)(), int _smoothingTime = 0, int _smoothingFrequency = 0) :
		EffectDefinition(false, effectFactory, _smoothingTime, _smoothingFrequency)
	{		
	}

	EffectDefinition(bool _directMode, AnimationBase* (*effectFactory)(), int _smoothingTime = 0, int _smoothingFrequency = 0) :
		smoothingDirectMode(_directMode),
		smoothingCustomSettings(_smoothingFrequency != 0 || _directMode),
		smoothingTime(_smoothingTime),
		smoothingFrequency(_smoothingFrequency),
		smoothingConfig(0),
		factory(effectFactory)
	{

	}
};
