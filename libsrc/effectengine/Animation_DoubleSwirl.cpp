#include <effectengine/Animation_DoubleSwirl.h>

Animation_DoubleSwirl::Animation_DoubleSwirl(QString name) :
	Animation_Swirl(name)
{
	custom_colors.clear();
	custom_colors2.clear();

	center_x = 0.5;
	center_x2 = 0.5;
	center_y = 0.5;
	center_y2 = 0.5;

	custom_colors.append({ 255,0,0 });
    custom_colors.append({20,0,255 });
        
       
    custom_colors2.append({255,0,0,0 });
    custom_colors2.append({255,0,0,0 });
    custom_colors2.append({255,0,0,0 });
    custom_colors2.append({0,0,0,1 });
    custom_colors2.append({0,0,0,0 });
    custom_colors2.append({255,0,0,0 });
    custom_colors2.append({255,0,0,0 });
    custom_colors2.append({0,0,0,1 });
       
	enable_second = true;
	random_center = false;
	random_center2 = false;
	reverse = false;
	reverse2 = true;
	rotation_time = 25;
}

EffectDefinition Animation_DoubleSwirl::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_DOUBLE_SWIRL;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_DoubleSwirl::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
