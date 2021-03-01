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

void MovingTarget::Clear()
{
	_averageColor = QColor(0, 0, 0);
	_fastColor = QColor(0, 0, 0);
	_slowColor = QColor(0, 0, 0);
	_targetAverageR = 0;
	_targetAverageG = 0;
	_targetAverageB = 0;
	_targetAverageCounter = 0;
	_targetSlowR = 0;
	_targetSlowG = 0;
	_targetSlowB = 0;
	_targetSlowCounter = 0;
	_targetFastR = 0;
	_targetFastG = 0;
	_targetFastB = 0;
	_targetFastCounter = 0;
};


