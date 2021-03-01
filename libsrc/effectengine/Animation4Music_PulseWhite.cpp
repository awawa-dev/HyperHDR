#include <effectengine/Animation4Music_PulseWhite.h>
#include <hyperhdrbase/SoundCapture.h>

Animation4Music_PulseWhite::Animation4Music_PulseWhite() :
	AnimationBaseMusic(AMUSIC_PULSEWHITE)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_PulseWhite::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_PULSEWHITE;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_PulseWhite::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_PulseWhite::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_PulseWhite::hasOwnImage()
{
	return true;
};

bool Animation4Music_PulseWhite::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);	

	if (r==NULL || !newData)
		return false;	

	int value = r->getValue(_oldMulti);

	if (value < 0)
		return false;

	QColor selected(value, value, value);
	newImage.fastBox(0,0,newImage.width()-1,newImage.height()-1, selected.red(), selected.green(), selected.blue());

	return true;
};







