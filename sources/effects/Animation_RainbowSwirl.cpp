/* Animation_RainbowSwirl.cpp
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

#include <effects/Animation_RainbowSwirl.h>

Animation_RainbowSwirl::Animation_RainbowSwirl() :
	Animation_Swirl()
{
	custom_colors.clear();
	custom_colors2.clear();

	rotation_time = 20.0;
	center_x = 0.5;
	center_y = 0.5;
	reverse = false;
	random_center = false;
	enable_second = false;
}

EffectDefinition Animation_RainbowSwirl::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_RainbowSwirl>, 200, 25);
	ed.name = ANIM_RAINBOW_SWIRL;
	return ed;
}

bool Animation_RainbowSwirl::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_RainbowSwirl::getDefinition());

