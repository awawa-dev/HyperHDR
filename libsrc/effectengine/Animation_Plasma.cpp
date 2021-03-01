#include <effectengine/Animation_Plasma.h>

Animation_Plasma::Animation_Plasma() :
	AnimationBase(ANIM_PLASMA)
{
	start = QDateTime::currentMSecsSinceEpoch();

	for(int h = 0; h < PAL_LEN; h++)
	{
		pal[h * 3] = 0;
		pal[h * 3+1] = 0;
		pal[h * 3+2] = 0;
	}

	for (int y = 0; y < PLASMA_HEIGHT; y++)
	for (int x = 0; x < PLASMA_WIDTH; x++)
	{
		int delta = y * PLASMA_WIDTH;
		plasma[delta + x] = 0;
	}
};

EffectDefinition Animation_Plasma::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_PLASMA;
	ed.args = GetArgs();
	return ed;
}


double  mapto(double x,double  in_min,double  in_max, double out_min, double out_max){
	return double((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

void Animation_Plasma::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{
	
	hyperImage = hyperImage.scaled(PLASMA_WIDTH, PLASMA_HEIGHT);
	
	SetSleepTime(int(round(0.1*1000.0)));
	
	for(int h = 0; h < PAL_LEN; h++)
	{
		uint8_t red, green, blue;
		red = green = blue = 0;
		ColorSys::hsv2rgb(h, 255, 255, red, green, blue);
		pal[h * 3] = red;
		pal[h * 3+1] = green;
		pal[h * 3+2] = blue;
	}

	
	for (int y = 0; y < PLASMA_HEIGHT; y++)
		for (int x = 0; x < PLASMA_WIDTH; x++) {
			int delta = y * PLASMA_WIDTH;
			plasma[delta + x] = int(128.0 + (128.0 * sin(x / 16.0)) + \
				128.0 + (128.0 * sin(y / 8.0)) + \
				128.0 + (128.0 * sin((x + y)) / 16.0) + \
				128.0 + (128.0 * sin(sqrt(pow (x,2.0) + pow(y,2.0)) / 8.0))) / 4;			
		}

	start = 0;
}




bool Animation_Plasma::Play(QPainter* painter)
{
	bool ret = true;
	
	qint64 mod = start++;
	for (int y = 0; y < PLASMA_HEIGHT; y++)
		for (int x = 0; x < PLASMA_WIDTH; x++) {
			int delta = y * PLASMA_WIDTH;
			int palIndex = int((plasma[delta + x] + mod) % PAL_LEN)*3;			

			painter->setPen(qRgb(pal[palIndex],pal[palIndex+1], pal[palIndex+2]));
			painter->drawPoint(x, y);
		}
	return ret;
}

QJsonObject Animation_Plasma::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}





