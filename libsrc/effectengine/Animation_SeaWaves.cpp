#include <effectengine/Animation_SeaWaves.h>

Animation_SeaWaves::Animation_SeaWaves(QString name) :
	Animation_Waves(name)
{
	custom_colors.clear();

	center_x = 1.25;
	center_y = -0.25;

	custom_colors.append({ 8,0,255 });
		custom_colors.append({ 4,80,255 });
	custom_colors.append({ 0,161,255 });
		custom_colors.append({ 0,191,255 });
	custom_colors.append({ 0,222,255 });
		custom_colors.append({ 0,188,255 });
	custom_colors.append({ 0,153,255 });
		custom_colors.append({ 19,76,255 });
	custom_colors.append({ 38,0,255 });
		custom_colors.append({ 19,100,255 });
	custom_colors.append({ 0,199,255 });

    random_center = false;
	reverse = false;
	reverse_time = 0;
	rotation_time = 60;
}

EffectDefinition Animation_SeaWaves::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_SEAWAVES;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_SeaWaves::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = true;
	doc["smoothing-time_ms"] = 200;
	doc["smoothing-updateFrequency"] = 25.0;
	return doc;
}
