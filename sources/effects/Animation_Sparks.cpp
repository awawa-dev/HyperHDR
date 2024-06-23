/* Animation_Sparks.cpp
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

#include <effects/Animation_Sparks.h>

Animation_Sparks::Animation_Sparks() :
	AnimationBase()
{
	sleep_time = 0.05;
	color = { 255, 255, 255 };
};

void Animation_Sparks::Init(
	HyperImage& hyperImage,
	int hyperLatchTime
)
{
	SetSleepTime((int)(sleep_time * 1000.0));
}

bool Animation_Sparks::Play(HyperImage& painter)
{
	return true;
}

bool Animation_Sparks::hasLedData(std::vector<ColorRgb>& buffer)
{
	ColorRgb newColor{ color.x, color.y, color.z };
	ColorRgb blackColor{ 0, 0, 0 };

	if (buffer.size() > 1)
	{
		ledData.clear();

		for (int i = 0; i < static_cast<int>(buffer.size()); i++)
		{
			if ((rand() % 1000) < 5)
				ledData.push_back(newColor);
			else
				ledData.push_back(blackColor);
		}

		ledData.swap(buffer);
	}

	return true;
}

EffectDefinition Animation_Sparks::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_Sparks>);
	ed.name = ANIM_SPARKS;
	return ed;
}

bool Animation_Sparks::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_Sparks::getDefinition());
