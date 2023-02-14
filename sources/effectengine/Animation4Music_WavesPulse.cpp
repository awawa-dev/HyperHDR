/* Animation4Music_WavesPulse.cpp
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

#include <effectengine/Animation4Music_WavesPulse.h>
#include <base/SoundCapture.h>

Animation4Music_WavesPulse::Animation4Music_WavesPulse() :
	AnimationBaseMusic(AMUSIC_WAVESPULSE),
	_internalIndex(0),
	_oldMulti(0)
{
	for (int i = 0; i < 5; i++)
	{
		_buffer.push_back(QColor(0, 0, 0));
	}
};

EffectDefinition Animation4Music_WavesPulse::getDefinition()
{
	EffectDefinition ed;
	ed.name = AMUSIC_WAVESPULSE;
	ed.args = GetArgs();
	return ed;
}

void Animation4Music_WavesPulse::Init(
	QImage& hyperImage,
	int hyperLatchTime
)
{
	SetSleepTime(15);
}

bool Animation4Music_WavesPulse::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_WavesPulse::hasOwnImage()
{
	return true;
};

bool Animation4Music_WavesPulse::getImage(Image<ColorRgb>& newImage)
{
	bool newData = false;
	auto r = SoundCapture::getInstance()->hasResult(this, _internalIndex, &newData, NULL, NULL, &_oldMulti);

	if (r == NULL || !newData)
		return false;

	QColor selected;
	uint32_t maxSingle, average;

	newImage.clear();

	if (!r->GetStats(average, maxSingle, selected))
		return false;

	//r->RestoreFullLum(selected);

	_buffer.pop_front();
	_buffer.push_back(selected);

	for (int y = 0, scaleY = 15; y <= scaleY / 2; y++)
	{
		int hm = (newImage.height() / scaleY);
		int h1 = (newImage.height() * y) / scaleY;
		int h2 = (newImage.height() * (scaleY - y)) / scaleY;
		int width = newImage.width() * 0.04;

		int w1, w2, ind = (y * (_buffer.length() - 1)) / (scaleY / 2);
		if (ind > _buffer.length() - 2)
		{
			ind = _buffer.length() - 2;
			w1 = 0; w2 = 100;
		}
		else
		{
			w2 = std::min((int)((y * (_buffer.length() - 1)) % (scaleY / 2)) * 100 / (scaleY / 2), (int)100);
			w1 = 100 - w2;
		}
		selected = QColor::fromRgb(
			std::min((_buffer[ind].red() * w1 + _buffer[ind + 1].red() * w2) / 100, 255),
			std::min((_buffer[ind].green() * w1 + _buffer[ind + 1].green() * w2) / 100, 255),
			std::min((_buffer[ind].blue() * w1 + _buffer[ind + 1].blue() * w2) / 100, 255)
		);

		newImage.gradientVBox(0, h1, width, h1 + hm, selected.red(), selected.green(), selected.blue());
		if (y != scaleY / 2)
			newImage.gradientVBox(0, h2, width, h2 - hm, selected.red(), selected.green(), selected.blue());

		newImage.gradientVBox(newImage.width() - 1 - width, h1, newImage.width() - 1, h1 + hm, selected.red(), selected.green(), selected.blue());
		if (y != scaleY / 2)
			newImage.gradientVBox(newImage.width() - 1 - width, h2, newImage.width() - 1, h2 - hm, selected.red(), selected.green(), selected.blue());
	}

	for (int x = 0, scaleX = 27; x <= scaleX / 2; x++)
	{
		int wm = (newImage.width() / scaleX);
		int w1 = (newImage.width() * x) / scaleX;
		int w2 = (newImage.width() * (scaleX - x)) / scaleX;
		int height = newImage.height() * 0.067;

		int we1, we2, ind = (x * (_buffer.length() - 1)) / (scaleX / 2);
		if (ind > _buffer.length() - 2)
		{
			ind = _buffer.length() - 2;
			we1 = 0; we2 = 100;
		}
		else
		{
			we2 = std::min((int)((x * (_buffer.length() - 1)) % (scaleX / 2)) * 100 / (scaleX / 2), (int)100);
			we1 = 100 - we2;
		}
		selected = QColor::fromRgb(
			std::min((_buffer[ind].red() * we1 + _buffer[ind + 1].red() * we2) / 100, 255),
			std::min((_buffer[ind].green() * we1 + _buffer[ind + 1].green() * we2) / 100, 255),
			std::min((_buffer[ind].blue() * we1 + _buffer[ind + 1].blue() * we2) / 100, 255)
		);

		newImage.gradientHBox(w1, 0, w1 + wm + 1, height, selected.red(), selected.green(), selected.blue());
		if (x != scaleX / 2)
			newImage.gradientHBox(w2, 0, w2 - wm, height, selected.red(), selected.green(), selected.blue());

		newImage.gradientHBox(w1, newImage.height() - 1 - height, w1 + wm + 1, newImage.height() - 1, selected.red(), selected.green(), selected.blue());
		if (x != scaleX / 2)
			newImage.gradientHBox(w2, newImage.height() - 1 - height, w2 - wm, newImage.height() - 1, selected.red(), selected.green(), selected.blue());
	}

	return true;

	return true;
};







