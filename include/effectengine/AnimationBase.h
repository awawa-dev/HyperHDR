#pragma once

#ifndef PCH_ENABLED
	#include <cmath>
	#include <QPainter>
#endif

#include <QPainter>

#include <utils/Image.h>
#include <effectengine/EffectDefinition.h>
#include <utils/ColorSys.h>

template <typename T>
AnimationBase* EffectFactory()
{
	return new T();
}

class AnimationBase
{
public:
	AnimationBase(QString name);
	virtual ~AnimationBase() = default;
	QString GetName();
	virtual bool Play(QPainter* painter) = 0;
	virtual void Init(QImage& hyperImage, int hyperLatchTime) = 0;
	virtual bool hasLedData(QVector<ColorRgb>& buffer);
	bool		 isStop();
	void		 SetSleepTime(int sleepTime);
	int			 GetSleepTime();
	virtual bool isSoundEffect();
	virtual bool hasOwnImage();
	virtual bool getImage(Image<ColorRgb>& image);

private:
	QString _name;
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

