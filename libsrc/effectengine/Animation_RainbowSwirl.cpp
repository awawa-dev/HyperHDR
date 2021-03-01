#include <effectengine/Animation_RainbowSwirl.h>

Animation_RainbowSwirl::Animation_RainbowSwirl(QString name) :
	Animation_Swirl(name)
{
	custom_colors.clear();
	custom_colors2.clear();

	rotation_time = 20.0;
	center_x = 0.5;
	center_y = 0.5;
	reverse = false;
	random_center = false;
	enable_second = false;
}

EffectDefinition Animation_RainbowSwirl::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_RAINBOW_SWIRL;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_RainbowSwirl::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = true;
	doc["smoothing-time_ms"] = 200;
	doc["smoothing-updateFrequency"] = 25.0;
	return doc;
}
