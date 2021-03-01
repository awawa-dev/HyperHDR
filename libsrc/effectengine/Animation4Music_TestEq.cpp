#include <effectengine/Animation4Music_TestEq.h>
#include <hyperhdrbase/SoundCapture.h>

Animation4Music_TestEq::Animation4Music_TestEq() :
	AnimationBaseMusic(AMUSIC_TESTEQ)	,
	_internalIndex(0)
{
	
};

EffectDefinition Animation4Music_TestEq::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_TESTEQ;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_TestEq::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(5);
}

bool Animation4Music_TestEq::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_TestEq::hasOwnImage()
{
	return true;
};

bool Animation4Music_TestEq::getImage(Image<ColorRgb>& newImage)
{
	uint8_t  buffScaledResult[SOUNDCAP_RESULT_RES];
	auto r = SoundCapture::getInstance()->hasResult(_internalIndex);	

	if (r==NULL)
		return false;

	r->GetBufResult(buffScaledResult, sizeof(buffScaledResult));


	memset(newImage.memptr(), 0, (size_t)newImage.width() * newImage.height() * 3);

	
	int delta = newImage.width() / SOUNDCAP_RESULT_RES;
	for (int i = 0; i < SOUNDCAP_RESULT_RES; i++)
	{
		QColor color = r->getRangeColor(i);
		uint16_t destX1 = (newImage.width()*i) / SOUNDCAP_RESULT_RES;
		uint16_t destX2 = destX1 + delta;
		uint16_t destY1 = newImage.height() - 1;
		uint16_t destY2 = destY1 * (255 - buffScaledResult[i]) / 255;
		newImage.fastBox(destX1, destY1 , destX2, destY2, color.red(), color.green(), color.blue());
	}
	return true;
};







