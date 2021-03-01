#include <effectengine/Animation4Music_PulseMultiFast.h>
#include <hyperhdrbase/SoundCapture.h>

Animation4Music_PulseMultiFast::Animation4Music_PulseMultiFast() :
	AnimationBaseMusic(AMUSIC_PULSEMULTIFAST)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_PulseMultiFast::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_PULSEMULTIFAST;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_PulseMultiFast::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_PulseMultiFast::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_PulseMultiFast::hasOwnImage()
{
	return true;
};

bool Animation4Music_PulseMultiFast::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	QColor color, empty;
	uint32_t maxSingle, average;

	if (!r->GetStats(average, maxSingle, empty, &color))
		return false;
	
	int red = color.red();
	int green = color.green();
	int blue = color.blue();

	newImage.fastBox(0,0,newImage.width()-1,newImage.height()-1, red, green, blue);

	return true;
};







