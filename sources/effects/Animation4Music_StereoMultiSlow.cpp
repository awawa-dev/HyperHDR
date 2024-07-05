/* Animation4Music_StereoMultiSlow.cpp
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

#include <effects/Animation4Music_StereoMultiSlow.h>

Animation4Music_StereoMultiSlow::Animation4Music_StereoMultiSlow() :
	AnimationBaseMusic(),
	_internalIndex(0),
	_oldMulti(0)
{

};

EffectDefinition Animation4Music_StereoMultiSlow::getDefinition()
{
	EffectDefinition ed(true, EffectFactory<Animation4Music_StereoMultiSlow>);
	ed.name = AMUSIC_STEREOMULTISLOW;
	return ed;
}

bool Animation4Music_StereoMultiSlow::Play(HyperImage& painter)
{
	return false;
}

bool Animation4Music_StereoMultiSlow::hasOwnImage()
{
	return true;
};

bool Animation4Music_StereoMultiSlow::getImage(Image<ColorRgb>& newImage)
{
	if (hasResult == nullptr)
		return false;

	bool newData = false;
	auto r = hasResult(this, _internalIndex, NULL, &newData, NULL, &_oldMulti);

	if (r == nullptr || !newData)
		return false;

	ColorRgb selected, empty;
	uint32_t maxSingle, average;

	newImage.clear();

	if (!r->GetStats(average, maxSingle, empty, NULL, &selected))
		return false;

	r->RestoreFullLum(selected);

	int value = r->getValue3Step(_oldMulti);

	int hm = (newImage.height() / 2);
	int h = std::min((hm * value) / 255, 255);
	int h1 = std::max(hm - h, 0);
	int h2 = std::min(hm + h, (int)newImage.height() - 1);

	if (h2 > h1)
	{
		int width = newImage.width() * 0.04;

		newImage.gradientVBox(0, h1, width, h2, selected.Red(), selected.Green(), selected.Blue());
		newImage.gradientVBox(newImage.width() - 1 - width, h1, newImage.width() - 1, h2, selected.Red(), selected.Green(), selected.Blue());
	}

	return true;
};

bool Animation4Music_StereoMultiSlow::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation4Music_StereoMultiSlow::getDefinition());
