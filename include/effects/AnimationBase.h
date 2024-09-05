#pragma once

#ifndef PCH_ENABLED
	#include <cmath>
#endif

#include <hyperimage/HyperImage.h>

#include <image/Image.h>
#include <effects/EffectDefinition.h>
#include <effects/EffectManufactory.h>

template <typename T>
AnimationBase* EffectFactory()
{
	return new T();
}

class AnimationBase
{
public:
	AnimationBase();
	virtual ~AnimationBase() = default;
	virtual bool Play(HyperImage& painter) = 0;
	virtual void Init(HyperImage& hyperImage, int hyperLatchTime) = 0;
	virtual bool hasLedData(std::vector<ColorRgb>& buffer);
	bool		 isStop();
	void		 SetSleepTime(int sleepTime);
	int			 GetSleepTime();
	virtual bool isSoundEffect();
	virtual bool hasOwnImage();
	virtual bool getImage(Image<ColorRgb>& image);

private:
	int     _sleepTime;
	bool    _stopMe;
protected:
	void setStopMe(bool stopMe);
	int clamp(int v, int lo, int hi);
};

struct Point2d
{
	int x, y;
};

struct Point3d
{
	uint8_t x, y, z;
};

struct Point3dhsv
{
	uint16_t x;
	uint8_t y, z;
};

