/* Animation4Music_WavesPulse.cpp
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

#include <effects/Animation4Music_WavesPulse.h>

Animation4Music_WavesPulse::Animation4Music_WavesPulse() :
	AnimationBaseMusic(),
	_internalIndex(0),
	_oldMulti(0)
{
	for (int i = 0; i < 5; i++)
	{
		_buffer.push_back(ColorRgb(0, 0, 0));
	}
};

EffectDefinition Animation4Music_WavesPulse::getDefinition()
{
	EffectDefinition ed(true, EffectFactory<Animation4Music_WavesPulse>);
	ed.name = AMUSIC_WAVESPULSE;
	return ed;
}

bool Animation4Music_WavesPulse::Play(HyperImage& painter)
{
	return false;
}

bool Animation4Music_WavesPulse::hasOwnImage()
{
	return true;
};

bool Animation4Music_WavesPulse::getImage(Image<ColorRgb>& newImage)
{
	if (hasResult == nullptr)
		return false;

	bool newData = false;
	auto r = hasResult(this, _internalIndex, &newData, NULL, NULL, &_oldMulti);

	if (r == nullptr || !newData)
		return false;

	ColorRgb selected;
	uint32_t maxSingle, average;

	newImage.clear();

	if (!r->GetStats(average, maxSingle, selected))
		return false;

	//r->RestoreFullLum(selected);

	_buffer.erase(_buffer.begin());
	_buffer.push_back(selected);

	for (int y = 0, scaleY = 15; y <= scaleY / 2; y++)
	{
		int hm = (newImage.height() / scaleY);
		int h1 = (newImage.height() * y) / scaleY;
		int h2 = (newImage.height() * (scaleY - y)) / scaleY;
		int width = newImage.width() * 0.04;

		int w1, w2, ind = (y * (_buffer.size() - 1)) / (scaleY / 2);
		if (ind > static_cast<int>(_buffer.size() - 2))
		{
			ind = _buffer.size() - 2;
			w1 = 0; w2 = 100;
		}
		else
		{
			w2 = std::min((int)((y * (_buffer.size() - 1)) % (scaleY / 2)) * 100 / (scaleY / 2), (int)100);
			w1 = 100 - w2;
		}
		selected = ColorRgb(
			std::min((_buffer[ind].Red() * w1 + _buffer[ind + 1].Red() * w2) / 100, 255),
			std::min((_buffer[ind].Green() * w1 + _buffer[ind + 1].Green() * w2) / 100, 255),
			std::min((_buffer[ind].Blue() * w1 + _buffer[ind + 1].Blue() * w2) / 100, 255)
		);

		newImage.gradientVBox(0, h1, width, h1 + hm, selected.Red(), selected.Green(), selected.Blue());
		if (y != scaleY / 2)
			newImage.gradientVBox(0, h2, width, h2 - hm, selected.Red(), selected.Green(), selected.Blue());

		newImage.gradientVBox(newImage.width() - 1 - width, h1, newImage.width() - 1, h1 + hm, selected.Red(), selected.Green(), selected.Blue());
		if (y != scaleY / 2)
			newImage.gradientVBox(newImage.width() - 1 - width, h2, newImage.width() - 1, h2 - hm, selected.Red(), selected.Green(), selected.Blue());
	}

	for (int x = 0, scaleX = 27; x <= scaleX / 2; x++)
	{
		int wm = (newImage.width() / scaleX);
		int w1 = (newImage.width() * x) / scaleX;
		int w2 = (newImage.width() * (scaleX - x)) / scaleX;
		int height = newImage.height() * 0.067;

		int we1, we2, ind = (x * (_buffer.size() - 1)) / (scaleX / 2);
		if (ind > static_cast<int>(_buffer.size() - 2))
		{
			ind = _buffer.size() - 2;
			we1 = 0; we2 = 100;
		}
		else
		{
			we2 = std::min((int)((x * (_buffer.size() - 1)) % (scaleX / 2)) * 100 / (scaleX / 2), (int)100);
			we1 = 100 - we2;
		}
		selected = ColorRgb(
			std::min((_buffer[ind].Red() * we1 + _buffer[ind + 1].Red() * we2) / 100, 255),
			std::min((_buffer[ind].Green() * we1 + _buffer[ind + 1].Green() * we2) / 100, 255),
			std::min((_buffer[ind].Blue() * we1 + _buffer[ind + 1].Blue() * we2) / 100, 255)
		);

		newImage.gradientHBox(w1, 0, w1 + wm + 1, height, selected.Red(), selected.Green(), selected.Blue());
		if (x != scaleX / 2)
			newImage.gradientHBox(w2, 0, w2 - wm, height, selected.Red(), selected.Green(), selected.Blue());

		newImage.gradientHBox(w1, newImage.height() - 1 - height, w1 + wm + 1, newImage.height() - 1, selected.Red(), selected.Green(), selected.Blue());
		if (x != scaleX / 2)
			newImage.gradientHBox(w2, newImage.height() - 1 - height, w2 - wm, newImage.height() - 1, selected.Red(), selected.Green(), selected.Blue());
	}

	return true;

	return true;
};

bool Animation4Music_WavesPulse::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation4Music_WavesPulse::getDefinition());
