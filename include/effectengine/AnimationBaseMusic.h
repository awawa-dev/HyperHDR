#pragma once
#include <stdlib.h>
#include <math.h>
#include <algorithm>

#include <QDateTime>
#include <QSize>
#include <QImage>
#include <QPainter>
#include <QJsonObject>

#include <utils/Image.h>
#include <effectengine/EffectDefinition.h>
#include <utils/ColorSys.h>
#include <effectengine/AnimationBase.h>

struct MovingTarget
{
	QColor	 _averageColor;
	QColor	 _fastColor;
	QColor	 _slowColor;
	int32_t _targetAverageR;
	int32_t _targetAverageG;
	int32_t _targetAverageB;
	int32_t _targetAverageCounter;
	int32_t _targetSlowR;
	int32_t _targetSlowG;
	int32_t _targetSlowB;
	int32_t _targetSlowCounter;
	int32_t _targetFastR;
	int32_t _targetFastG;
	int32_t _targetFastB;
	int32_t _targetFastCounter;
};

class AnimationBaseMusic : public AnimationBase
{
	Q_OBJECT

public:
	AnimationBaseMusic(QString name);
	static QJsonObject GetArgs();

	bool isSoundEffect() override;

	void store(MovingTarget* source);
	void restore(MovingTarget* target);

private:
	MovingTarget _myTarget;
protected:
	
};

