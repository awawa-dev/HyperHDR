#include <effectengine/AnimationBaseMusic.h>

AnimationBaseMusic::AnimationBaseMusic(QString name) :
	AnimationBase(name)	
{
	memset(&_myTarget, 0, sizeof(_myTarget));
};

QJsonObject AnimationBaseMusic::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = true;
	doc["smoothing-direct-mode"] = true;
	return doc;
}

bool AnimationBaseMusic::isSoundEffect()
{
	return true;
};

void AnimationBaseMusic::store(MovingTarget* source) {
	memcpy(&_myTarget, source, sizeof(_myTarget));
};

void AnimationBaseMusic::restore(MovingTarget* target) {
	memcpy(target, &_myTarget, sizeof(_myTarget));
};

