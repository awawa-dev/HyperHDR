/* Animation_Plasma.cpp
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
#include <utils/InternalClock.h>
#include <effects/Animation_Plasma.h>

Animation_Plasma::Animation_Plasma() :
	AnimationBase()
{
	start = InternalClock::now();

	for (int h = 0; h < PAL_LEN; h++)
	{
		pal[h * 3] = 0;
		pal[h * 3 + 1] = 0;
		pal[h * 3 + 2] = 0;
	}

	for (int y = 0; y < PLASMA_HEIGHT; y++)
		for (int x = 0; x < PLASMA_WIDTH; x++)
		{
			int delta = y * PLASMA_WIDTH;
			plasma[delta + x] = 0;
		}
};

EffectDefinition Animation_Plasma::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_Plasma>);
	ed.name = ANIM_PLASMA;
	return ed;
}


double  mapto(double x, double  in_min, double  in_max, double out_min, double out_max) {
	return double((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

void Animation_Plasma::Init(
	HyperImage& hyperImage,
	int hyperLatchTime
)
{

	hyperImage.resize(PLASMA_WIDTH, PLASMA_HEIGHT);

	SetSleepTime(int(round(0.1 * 1000.0)));

	for (int h = 0; h < PAL_LEN; h++)
	{
		ColorRgb c;
		c.fromHsv(h, 255, 255);
		pal[h * 3] = c.red;
		pal[h * 3 + 1] = c.green;
		pal[h * 3 + 2] = c.blue;
	}


	for (int y = 0; y < PLASMA_HEIGHT; y++)
		for (int x = 0; x < PLASMA_WIDTH; x++) {
			int delta = y * PLASMA_WIDTH;
			plasma[delta + x] = int(128.0 + (128.0 * sin(x / 16.0)) + \
				128.0 + (128.0 * sin(y / 8.0)) + \
				128.0 + (128.0 * sin((x + y)) / 16.0) + \
				128.0 + (128.0 * sin(sqrt(pow(x, 2.0) + pow(y, 2.0)) / 8.0))) / 4;
		}

	start = 0;
}




bool Animation_Plasma::Play(HyperImage& painter)
{
	bool ret = true;

	auto mod = start++;
	for (int y = 0; y < PLASMA_HEIGHT; y++)
		for (int x = 0; x < PLASMA_WIDTH; x++) {
			int delta = y * PLASMA_WIDTH;
			int palIndex = int((plasma[delta + x] + mod) % PAL_LEN) * 3;

			painter.setPen(ColorRgb(pal[palIndex], pal[palIndex + 1], pal[palIndex + 2]));
			painter.drawPoint(x, y);
		}
	return ret;
}


bool Animation_Plasma::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_Plasma::getDefinition());
