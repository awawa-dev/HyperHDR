#include <effectengine/Animation_CandleLight.h>

// Candleflicker effect by penfold42
// Algorithm courtesy of
// https://cpldcpu.com/2013/12/08/hacking-a-candleflicker-led/

Animation_CandleLight::Animation_CandleLight(QString name) :
	AnimationBase(name)
{
	colorShift = 4;	
	color = { 255, 138, 0 };
	hsv = { 0, 0, 0 };
};

void Animation_CandleLight::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{
	ColorSys::rgb2hsv(color.x, color.y, color.z, hsv.x, hsv.y, hsv.z );
	SetSleepTime((int)(0.20 * 1000.0));
}

bool Animation_CandleLight::Play(QPainter* painter)
{
	return true;
}

Point3d Animation_CandleLight::CandleRgb()
{
	Point3d frgb;
	int		RAND = (rand() % (colorShift*2+1)) - colorShift;
	int		hue = (hsv.x + RAND + 360) % 360;
	int		breakStop = 0;

	RAND = rand() % 16;
	while ((RAND & 0x0c) == 0 && (breakStop++ < 100)) {
		RAND = rand() % 16;
	}

	int val = std::min(RAND * 17, 255);

	ColorSys::hsv2rgb(hue, hsv.y, val, frgb.x, frgb.y, frgb.z);

	return frgb;
}

bool Animation_CandleLight::hasLedData(QVector<ColorRgb>& buffer)
{
	if (buffer.length() > 1)
	{
		ledData.clear();

		for (int i = 0; i < buffer.length(); i++)
		{
			Point3d  color = CandleRgb();
			ColorRgb newColor{ color.x, color.y, color.z };

			ledData.append(newColor);
		}

		ledData.swap(buffer);
	}

	return true;
}


