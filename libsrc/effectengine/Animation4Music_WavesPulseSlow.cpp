#include <effectengine/Animation4Music_WavesPulseSlow.h>
#include <hyperhdrbase/SoundCapture.h>

#include <cmath>

Animation4Music_WavesPulseSlow::Animation4Music_WavesPulseSlow() :
	AnimationBaseMusic(AMUSIC_WAVESPULSESLOW),
	_internalIndex(0),
	_oldMulti(0)
{
	for (int i = 0; i < 5; i++)
	{
		_buffer.push_back(QColor(0, 0, 0));
	}	
};

EffectDefinition Animation4Music_WavesPulseSlow::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_WAVESPULSESLOW;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_WavesPulseSlow::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_WavesPulseSlow::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_WavesPulseSlow::hasOwnImage()
{
	return true;
};


bool Animation4Music_WavesPulseSlow::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, &newData, NULL, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	QColor empty,selected;
	uint32_t maxSingle, average;

	memset(newImage.memptr(), 0, newImage.size());

	if (!r->GetStats(average, maxSingle, empty, NULL, &selected) )
		return false;

	//r->RestoreFullLum(selected);

	_buffer.pop_front();
	_buffer.push_back(selected);
			
	for (int y = 0, scaleY = 15; y <= scaleY/2 ; y++)
	{
		int hm     = (newImage.height() / scaleY);
		int h1     = (newImage.height() * y ) / scaleY;
		int h2     = (newImage.height() * (scaleY - y) ) / scaleY;
		int width  = newImage.width() * 0.04;

		int w1,w2,ind = ( y * (_buffer.length() - 1)) / (scaleY / 2);
		if (ind > _buffer.length() - 2)
		{
			ind = _buffer.length() - 2;
			w1 = 0; w2 = 100;
		}
		else
		{
			w2 = std::min((int) (( y * (_buffer.length() - 1)) % (scaleY / 2))*100/(scaleY / 2), (int)100);
			w1 = 100 - w2;
		}
		selected = QColor::fromRgb(
			std::min((_buffer[ind].red() * w1   + _buffer[ind + 1].red()   * w2)/100,255),
			std::min((_buffer[ind].green() * w1 + _buffer[ind + 1].green() * w2)/100,255),
			std::min((_buffer[ind].blue() * w1  + _buffer[ind + 1].blue()  * w2)/100,255)
		);
			
		newImage.gradientVBox(0, h1, width, h1 + hm, selected.red(), selected.green(), selected.blue());
		if (y != scaleY/2)
			newImage.gradientVBox(0, h2, width, h2 - hm, selected.red(), selected.green(), selected.blue());

		newImage.gradientVBox(newImage.width() - 1 - width, h1, newImage.width() - 1, h1 + hm, selected.red(), selected.green(), selected.blue());
		if (y != scaleY/2)
			newImage.gradientVBox(newImage.width() - 1 - width, h2, newImage.width() - 1, h2 - hm, selected.red(), selected.green(), selected.blue());
	}

	for (int x = 0, scaleX = 27; x <= scaleX/2 ; x++)
	{
		int wm     = (newImage.width() / scaleX);
		int w1     = (newImage.width() * x ) / scaleX;
		int w2     = (newImage.width() * (scaleX - x) ) / scaleX;
		int height = newImage.height() * 0.067;

		int we1,we2,ind = ( x * (_buffer.length() - 1)) / (scaleX / 2);
		if (ind > _buffer.length() - 2)
		{
			ind = _buffer.length() - 2;
			we1 = 0; we2 = 100;
		}
		else
		{
			we2 = std::min((int)(( x * (_buffer.length() - 1)) % (scaleX / 2))*100/(scaleX / 2), (int)100);
			we1 = 100 - we2;
		}
		selected = QColor::fromRgb(
			std::min((_buffer[ind].red() * we1   + _buffer[ind + 1].red()   * we2)/100,255),
			std::min((_buffer[ind].green() * we1 + _buffer[ind + 1].green() * we2)/100,255),
			std::min((_buffer[ind].blue() * we1  + _buffer[ind + 1].blue()  * we2)/100,255)
		);
			
		newImage.gradientHBox(w1, 0, w1 + wm + 1, height, selected.red(), selected.green(), selected.blue());
		if (x != scaleX/2)
			newImage.gradientHBox(w2, 0, w2 - wm, height, selected.red(), selected.green(), selected.blue());

		newImage.gradientHBox(w1, newImage.height() - 1 - height, w1 + wm + 1, newImage.height() - 1, selected.red(), selected.green(), selected.blue());
		if (x != scaleX/2)
			newImage.gradientHBox(w2, newImage.height() - 1 - height, w2 - wm, newImage.height() - 1, selected.red(), selected.green(), selected.blue());
	}		

	return true;
};







