#include <effectengine/Animation4Music_PulseYellow.h>
#include <hyperhdrbase/SoundCapture.h>

Animation4Music_PulseYellow::Animation4Music_PulseYellow() :
	AnimationBaseMusic(AMUSIC_PULSEYELLOW)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_PulseYellow::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_PULSEYELLOW;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_PulseYellow::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_PulseYellow::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_PulseYellow::hasOwnImage()
{
	return true;
};

bool Animation4Music_PulseYellow::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	int value = r->getValue(_oldMulti);

	if (value < 0)
		return false;
	
	QColor selected(value, value, 0);
	newImage.fastBox(0,0,newImage.width()-1,newImage.height()-1, selected.red(), selected.green(), selected.blue());

	return true;
};







