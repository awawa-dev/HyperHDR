/* Animation_RainbowWaves.cpp
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

#include <effects/Animation_RainbowWaves.h>

Animation_RainbowWaves::Animation_RainbowWaves() :
	AnimationBase()
{
	SetSleepTime(100);
	hue = 0;
};



void Animation_RainbowWaves::Init(
	HyperImage& hyperImage,
	int hyperLatchTime
)
{

}

bool Animation_RainbowWaves::Play(HyperImage& painter)
{
	return true;
}

bool Animation_RainbowWaves::hasLedData(std::vector<ColorRgb>& buffer)
{
	if (buffer.size() > 0)
	{
		ColorRgb newColor{ 0,0,0 };
		ColorRgb::hsv2rgb(hue, 255, 255, newColor.red, newColor.green, newColor.blue);

		if (++hue > 359)
			hue = 0;

		for (int i = 0; i < static_cast<int>(buffer.size()); i++)
		{
			buffer[i] = newColor;
		}
	}

	return true;
}

EffectDefinition Animation_RainbowWaves::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_RainbowWaves>);
	ed.name = ANIM_RAINBOW_WAVES;
	return ed;
}

bool Animation_RainbowWaves::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_RainbowWaves::getDefinition());

