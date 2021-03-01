#include <effectengine/Animation_Police.h>

Animation_Police::Animation_Police(QString name) :
	AnimationBase(name)
{
	rotationTime = 2;
	colorOne = { 255, 0, 0 };
	colorTwo  = {  0, 0, 255 };
	reverse = false;	
	ledData.resize(0);
	colorsCount = 1000;
	increment = 1;
};



void Animation_Police::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{
	
}

bool Animation_Police::Play(QPainter* painter)
{
	return true;
}

bool Animation_Police::hasLedData(QVector<ColorRgb>& buffer)
{
	if (buffer.length() != ledData.length() && buffer.length() > 1)
	{
		int hledCount = buffer.length();
		ledData.clear();

		rotationTime = std::max(0.1, rotationTime);
		colorsCount = std::min(hledCount / 2, colorsCount);
		
		Point3d    hsv1, hsv2;
		uint16_t   hsv1x, hsv2x;

		ColorSys::rgb2hsv(colorOne.x, colorOne.y, colorOne.z, hsv1x, hsv1.y, hsv1.z);
		ColorSys::rgb2hsv(colorTwo.x, colorTwo.y, colorTwo.z, hsv2x, hsv2.y, hsv2.z);
		
		for(int i=0; i<hledCount; i++)
		{
			ColorRgb newColor{ 0,0,0 };
			if (i <= colorsCount)
				ColorSys::hsv2rgb(hsv1x, hsv1.y, hsv1.z, newColor.red, newColor.green, newColor.blue);
			else if ((i >= hledCount/2-1) && (i < (hledCount/2) + colorsCount))
				ColorSys::hsv2rgb(hsv2x, hsv2.y, hsv2.z, newColor.red, newColor.green, newColor.blue);
			ledData.append(newColor);
		}

		increment = 2;
		double sleepTime = rotationTime / hledCount;
		while (sleepTime < 0.05)
		{
			increment *= 2;
			sleepTime *= 2;
		}
		SetSleepTime(int(round(sleepTime*1000.0)));

		increment %= hledCount;
		ledData.swap(buffer);
	}

	if (buffer.length() == ledData.length())
	{
		if (reverse)
		{
			for (int i = 0; i < increment; i++)
				buffer.move(0, buffer.size() - 1);
		}
		else
		{
			for (int i = 0; i < increment; i++)
				buffer.move(buffer.size() - 1, 0);
		}

	}
	return true;
}


