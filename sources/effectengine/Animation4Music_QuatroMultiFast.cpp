/* Animation4Music_QuatroMultiFast.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <effectengine/Animation4Music_QuatroMultiFast.h>

Animation4Music_QuatroMultiFast::Animation4Music_QuatroMultiFast() :
	AnimationBaseMusic(AMUSIC_QUATROMULTIFAST),
	_internalIndex(0),
	_oldMulti(0)
{

};

EffectDefinition Animation4Music_QuatroMultiFast::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_QUATROMULTIFAST;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_QuatroMultiFast::Init(
	QImage& hyperImage,
	int hyperLatchTime
)
{
	SetSleepTime(15);
}

bool Animation4Music_QuatroMultiFast::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_QuatroMultiFast::hasOwnImage()
{
	return true;
};

bool Animation4Music_QuatroMultiFast::getImage(Image<ColorRgb>& newImage)
{
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);

	if (r == NULL || !newData)
		return false;

	QColor empty, selected;
	uint32_t maxSingle, average;

	memset(newImage.memptr(), 0, newImage.size());

	if (!r->GetStats(average, maxSingle, empty, &selected))
		return false;

	r->RestoreFullLum(selected);

	int value = r->getValue3Step(_oldMulti);

	{
		int hm = (newImage.height() / 2);
		int h = std::min((hm * value) / 255, 255);
		int h1 = std::max(hm - h, 0);
		int h2 = std::min(hm + h, (int)newImage.height() - 1);

		if (h2 > h1)
		{
			int width = newImage.width() * 0.04;

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

			newImage.gradientHBox(w1, 0, w2, height, selected.red(), selected.green(), selected.blue());
			newImage.gradientHBox(w1, newImage.height() - 1 - height, w2, newImage.height() - 1, selected.red(), selected.green(), selected.blue());
		}
	}

	return true;
};







