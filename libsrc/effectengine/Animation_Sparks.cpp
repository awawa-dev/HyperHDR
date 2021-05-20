#include <effectengine/Animation_Sparks.h>

Animation_Sparks::Animation_Sparks() :
	AnimationBase(ANIM_SPARKS)
{
	sleep_time = 0.05;
	color = { 255, 255, 255 };
};

void Animation_Sparks::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{
	SetSleepTime((int)(sleep_time * 1000.0));
}

bool Animation_Sparks::Play(QPainter* painter)
{
	return true;
}

bool Animation_Sparks::hasLedData(QVector<ColorRgb>& buffer)
{
	ColorRgb newColor{ color.x, color.y, color.z };
	ColorRgb blackColor{ 0, 0, 0 };

	if (buffer.length() > 1)
	{
		ledData.clear();

		for (int i = 0; i < buffer.length(); i++)
		{						
			if ((rand() % 1000) < 5)					
				ledData.append(newColor);
			else
				ledData.append(blackColor);
		}

		ledData.swap(buffer);
	}

	return true;
}

EffectDefinition Animation_Sparks::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_SPARKS;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_Sparks::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;	
	return doc;
}


