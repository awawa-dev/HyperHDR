#include <effectengine/Animation_SwirlFast.h>

Animation_SwirlFast::Animation_SwirlFast(QString name) :
	Animation_Swirl(name)
{
	custom_colors.clear();
	custom_colors2.clear();

	rotation_time = 7.0;
	center_x = 0.5;
	center_y = 0.5;
	reverse = false;
	random_center = false;	
	enable_second = false;
}

EffectDefinition Animation_SwirlFast::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_SWIRL_FAST;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_SwirlFast::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
