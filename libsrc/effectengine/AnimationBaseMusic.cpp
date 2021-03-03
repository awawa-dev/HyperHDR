#include <effectengine/AnimationBaseMusic.h>

AnimationBaseMusic::AnimationBaseMusic(QString name) :
	AnimationBase(name)	
{
	_myTarget.Clear();
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
	_myTarget.CopyFrom(source);
};

void AnimationBaseMusic::restore(MovingTarget* target) {
	target->CopyFrom(&_myTarget);
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

void MovingTarget::CopyFrom(MovingTarget* source)
{
	_averageColor = source->_averageColor;
	_fastColor = source->_fastColor;
	_slowColor = source->_slowColor;
	_targetAverageR = source->_targetAverageR;
	_targetAverageG = source->_targetAverageG;
	_targetAverageB = source->_targetAverageB;
	_targetAverageCounter = source->_targetAverageCounter;
	_targetSlowR = source->_targetSlowR;
	_targetSlowG = source->_targetSlowG;
	_targetSlowB = source->_targetSlowB;
	_targetSlowCounter = source->_targetSlowCounter;
	_targetFastR = source->_targetFastR;
	_targetFastG = source->_targetFastG;
	_targetFastB = source->_targetFastB;
	_targetFastCounter = source->_targetFastCounter;
};


