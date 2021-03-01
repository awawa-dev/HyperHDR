#include <effectengine/Animation_AtomicSwirl.h>

Animation_AtomicSwirl::Animation_AtomicSwirl(QString name) :
	Animation_Swirl(name)
{
	custom_colors.clear();
	custom_colors2.clear();


	rotation_time = 25.0;
	center_x = 0.5;
	center_y = 0.5;
	reverse = false;
	
	custom_colors.append({ 0,0,0 });
	custom_colors.append({ 0,0,0 });
	custom_colors.append({ 255,255,0 });
	custom_colors.append({ 0,0,0 });
	custom_colors.append({ 0,0,0 });
	custom_colors.append({ 255,255,0 });
	custom_colors.append({ 0,0,0 });
	custom_colors.append({ 0,0,0 });
	custom_colors.append({ 255,255,0 });

	random_center = false;
	enable_second = false;
}

EffectDefinition Animation_AtomicSwirl::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_ATOMIC_SWIRL;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_AtomicSwirl::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
