#pragma once

#include <effects/AnimationBase.h>

#define ANIM_SYSTEM_SHUTDOWN "System Shutdown"
#define SYSTEMSHUTDOWN_WIDTH  12
#define SYSTEMSHUTDOWN_HEIGHT 10

class Animation_SystemShutdown : public AnimationBase
{
public:

	Animation_SystemShutdown();

	void Init(
		HyperImage& hyperImage,
		int hyperLatchTime) override;

	bool Play(HyperImage& painter) override;

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

	void setLine(HyperImage& painter, int y, Point3d rgb);
	void setFill(HyperImage& painter, Point3d rgb);

private:
	static bool isRegistered;

};
