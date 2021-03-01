#include <effectengine/AnimationBase.h>

AnimationBase::AnimationBase(QString name) :
	_name(name),
	_sleepTime(100),
	_isDevice(false)
{
};

QString AnimationBase::GetName()
{
	return _name;
};

bool AnimationBase::isSoundEffect()
{
	return false;
};

bool AnimationBase::hasOwnImage()
{
	return false;
};

bool AnimationBase::getImage(Image<ColorRgb>& image)
{	
	return false;
};

void AnimationBase::SetSleepTime(int sleepTime)
{
	_sleepTime = sleepTime;
};

int  AnimationBase::GetSleepTime()
{
	return _sleepTime;
};

int AnimationBase::clamp( int v, int lo, int hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

bool AnimationBase::hasLedData(QVector<ColorRgb>& buffer)
{
	return false;
}

