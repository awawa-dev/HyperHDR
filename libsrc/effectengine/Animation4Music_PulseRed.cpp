#include <effectengine/Animation4Music_PulseRed.h>
#include <hyperhdrbase/SoundCapture.h>

Animation4Music_PulseRed::Animation4Music_PulseRed() :
	AnimationBaseMusic(AMUSIC_PULSERED)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_PulseRed::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_PULSERED;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_PulseRed::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_PulseRed::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_PulseRed::hasOwnImage()
{
	return true;
};

bool Animation4Music_PulseRed::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	int value = r->getValue(_oldMulti);

	if (value < 0)
		return false;

	QColor selected(value, 0, 0);
	newImage.fastBox(0,0,newImage.width()-1,newImage.height()-1, selected.red(), selected.green(), selected.blue());

	return true;
};







