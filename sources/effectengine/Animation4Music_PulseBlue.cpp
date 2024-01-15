/* Animation4Music_PulseBlue.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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

#include <effectengine/Animation4Music_PulseBlue.h>
#include <base/SoundCapture.h>

Animation4Music_PulseBlue::Animation4Music_PulseBlue() :
	AnimationBaseMusic(AMUSIC_PULSEBLUE),
	_internalIndex(0),
	_oldMulti(0)
{

};

EffectDefinition Animation4Music_PulseBlue::getDefinition()
{
	EffectDefinition ed(true, EffectFactory<Animation4Music_PulseBlue>);
	ed.name = AMUSIC_PULSEBLUE;
	return ed;
}

void Animation4Music_PulseBlue::Init(
	QImage& hyperImage,
	int hyperLatchTime
)
{
	SetSleepTime(15);
}

bool Animation4Music_PulseBlue::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_PulseBlue::hasOwnImage()
{
	return true;
};

bool Animation4Music_PulseBlue::getImage(Image<ColorRgb>& newImage)
{
	bool newData = false;
	auto r = _soundCapture->hasResult(this, _internalIndex, NULL, NULL, &newData, &_oldMulti);

	if (r == nullptr || !newData)
		return false;

	int value = r->getValue(_oldMulti);

	if (value < 0)
		return false;

	QColor selected(0, 0, value);
	newImage.fastBox(0, 0, newImage.width() - 1, newImage.height() - 1, selected.red(), selected.green(), selected.blue());

	return true;
};







