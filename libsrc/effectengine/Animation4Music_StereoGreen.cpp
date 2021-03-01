#include <effectengine/Animation4Music_StereoGreen.h>
#include <hyperhdrbase/SoundCapture.h>

#include <cmath>

Animation4Music_StereoGreen::Animation4Music_StereoGreen() :
	AnimationBaseMusic(AMUSIC_STEREOGREEN)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_StereoGreen::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_STEREOGREEN;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_StereoGreen::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_StereoGreen::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_StereoGreen::hasOwnImage()
{
	return true;
};

bool Animation4Music_StereoGreen::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	int value = r->getValue(_oldMulti);

	if (value < 0)
		return false;

	memset(newImage.memptr(), 0, newImage.size());

	int hm = (newImage.height() / 2);
	int h =  std::min( (hm * value) / 255, 255);
	int h1 = std::max( hm - h, 0);
	int h2 = std::min( hm + h, (int)newImage.height() - 1);
	
	if (h2 > h1)
	{
		int width = newImage.width() * 0.04;
		
		QColor selected(0, 255, 0);

		newImage.gradientVBox(0, h1,							width, h2,				  selected.red(), selected.green(), selected.blue());
		newImage.gradientVBox(newImage.width() - 1 - width,h1,  newImage.width() - 1, h2, selected.red(), selected.green(), selected.blue());
	}

	return true;
};







