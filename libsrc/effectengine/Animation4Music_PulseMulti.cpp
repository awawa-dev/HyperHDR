#include <effectengine/Animation4Music_PulseMulti.h>
#include <hyperhdrbase/SoundCapture.h>

Animation4Music_PulseMulti::Animation4Music_PulseMulti() :
	AnimationBaseMusic(AMUSIC_PULSEMULTI)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_PulseMulti::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_PULSEMULTI;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_PulseMulti::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_PulseMulti::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_PulseMulti::hasOwnImage()
{
	return true;
};

bool Animation4Music_PulseMulti::getImage(Image<ColorRgb>& newImage)
{
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, &newData, NULL, NULL, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	QColor color;
	uint32_t maxSingle, average;

	if (!r->GetStats(average, maxSingle, color))
		return false;
	
	int red = color.red();
	int green = color.green();
	int blue = color.blue();

	newImage.fastBox(0,0,newImage.width()-1,newImage.height()-1, red, green, blue);

	return true;
};







