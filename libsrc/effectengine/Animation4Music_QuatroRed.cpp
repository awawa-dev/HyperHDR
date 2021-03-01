#include <effectengine/Animation4Music_QuatroRed.h>
#include <hyperhdrbase/SoundCapture.h>

#include <cmath>

Animation4Music_QuatroRed::Animation4Music_QuatroRed() :
	AnimationBaseMusic(AMUSIC_QUATRORED)	,
	_internalIndex(0),
	_oldMulti(0)
{
	
};

EffectDefinition Animation4Music_QuatroRed::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_QUATRORED;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_QuatroRed::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{		
	SetSleepTime(15);
}

bool Animation4Music_QuatroRed::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_QuatroRed::hasOwnImage()
{
	return true;
};

bool Animation4Music_QuatroRed::getImage(Image<ColorRgb>& newImage)
{	
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);	

	if (r==NULL || !newData)
		return false;

	int value = r->getValue(_oldMulti);

	if (value < 0)
		return false;

	memset(newImage.memptr(), 0, newImage.size());

	{
		int hm = (newImage.height() / 2);
		int h = std::min((hm * value) / 255, 255);
		int h1 = std::max(hm - h, 0);
		int h2 = std::min(hm + h, (int)newImage.height() - 1);

		if (h2 > h1)
		{
			int width = newImage.width() * 0.04;

			QColor selected(255, 0, 0);

			newImage.gradientVBox(0, h1, width, h2, selected.red(), selected.green(), selected.blue());
			newImage.gradientVBox(newImage.width() - 1 - width, h1, newImage.width() - 1, h2, selected.red(), selected.green(), selected.blue());
		}
	}
	{
		int wm = (newImage.width() / 2);
		int w = std::min((wm * value) / 255, 255);
		int w1 = std::max(wm - w, 0);
		int w2 = std::min(wm + w, (int)newImage.width() - 1);

		if (w2 > w1)
		{
			int height = newImage.height() * 0.067;

			QColor selected(255, 0, 0);

			newImage.gradientHBox(w1, 0, w2, height, selected.red(), selected.green(), selected.blue());
			newImage.gradientHBox(w1, newImage.height() - 1 - height, w2, newImage.height() - 1, selected.red(), selected.green(), selected.blue());
		}
	}

	return true;
};







