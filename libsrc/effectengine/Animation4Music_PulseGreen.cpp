#include <effectengine/Animation4Music_PulseGreen.h>
#include <hyperhdrbase/SoundCapture.h>

Animation4Music_PulseGreen::Animation4Music_PulseGreen() :
	AnimationBaseMusic(AMUSIC_PULSEGREEN)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_PulseGreen::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_PULSEGREEN;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_PulseGreen::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_PulseGreen::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_PulseGreen::hasOwnImage()
{
	return true;
};

bool Animation4Music_PulseGreen::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	int value = r->getValue(_oldMulti);

	if (value < 0)
		return false;
	
	QColor selected(0, value, 0);
	newImage.fastBox(0,0,newImage.width()-1,newImage.height()-1, selected.red(), selected.green(), selected.blue());

	return true;
};







