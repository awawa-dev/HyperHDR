#pragma once

#include <effectengine/AnimationBase.h>

#define ANIM_SYSTEM_SHUTDOWN "System Shutdown"
#define SYSTEMSHUTDOWN_WIDTH  12
#define SYSTEMSHUTDOWN_HEIGHT 10

class Animation_SystemShutdown : public AnimationBase
{
public:

	Animation_SystemShutdown();

	void Init(
		QImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(QPainter* painter) override;

	static EffectDefinition getDefinition();

private:

	double speed;
	Point3d alarm_color;
	Point3d post_color;
	bool shutdown_enabled;
	bool initial_blink;
	bool set_post_color;
	int initial_blink_index;
	int yIndex;

	void setLine(QPainter* painter, int y, Point3d rgb);
	void setFill(QPainter* painter, Point3d rgb);
};
