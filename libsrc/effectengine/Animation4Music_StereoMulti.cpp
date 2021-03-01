#include <effectengine/Animation4Music_StereoMulti.h>
#include <hyperhdrbase/SoundCapture.h>

#include <cmath>

Animation4Music_StereoMulti::Animation4Music_StereoMulti() :
	AnimationBaseMusic(AMUSIC_STEREOMULTI)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_StereoMulti::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_STEREOMULTI;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_StereoMulti::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_StereoMulti::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_StereoMulti::hasOwnImage()
{
	return true;
};

bool Animation4Music_StereoMulti::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, &newData, NULL, NULL, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	QColor selected;
	uint32_t maxSingle, average;

	memset(newImage.memptr(), 0, newImage.size());

	if (!r->GetStats(average, maxSingle, selected))
		return false;

	r->RestoreFullLum(selected);

	int value = r->getValue3Step(_oldMulti);

	int hm = (newImage.height() / 2);
	int h =  std::min( (hm * value) / 255, 255);
	int h1 = std::max( hm - h, 0);
	int h2 = std::min( hm + h, (int)newImage.height() - 1);
	
	if (h2 > h1)
	{
		int width = newImage.width() * 0.04;
				
		newImage.gradientVBox(0, h1,							width, h2,				  selected.red(), selected.green(), selected.blue());
		newImage.gradientVBox(newImage.width() - 1 - width,h1,  newImage.width() - 1, h2, selected.red(), selected.green(), selected.blue());
	}

	return true;
};







