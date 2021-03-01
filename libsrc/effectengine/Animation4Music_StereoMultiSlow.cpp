#include <effectengine/Animation4Music_StereoMultiSlow.h>
#include <hyperhdrbase/SoundCapture.h>

#include <cmath>

Animation4Music_StereoMultiSlow::Animation4Music_StereoMultiSlow() :
	AnimationBaseMusic(AMUSIC_STEREOMULTISLOW)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_StereoMultiSlow::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_STEREOMULTISLOW;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_StereoMultiSlow::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_StereoMultiSlow::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_StereoMultiSlow::hasOwnImage()
{
	return true;
};

bool Animation4Music_StereoMultiSlow::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, &newData, NULL, &_oldMulti);

	if (r==NULL || !newData)
		return false;

	QColor selected, empty;
	uint32_t maxSingle, average;

	memset(newImage.memptr(), 0, newImage.size());

	if (!r->GetStats(average, maxSingle, empty, NULL, &selected) )
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







