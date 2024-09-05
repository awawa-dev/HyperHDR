/* Animation_NotifyBlue.cpp
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

#include <effects/Animation_NotifyBlue.h>

Animation_NotifyBlue::Animation_NotifyBlue() :
	Animation_Fade()
{
	colorStart = { 0, 0, 50 };
	colorEnd = { 0, 0, 255 };
	fadeInTime = 200;
	fadeOutTime = 100;
	colorStartTime = 40;
	colorEndTime = 150;
	repeat = 3;
	maintainEndCol = false;
}

EffectDefinition Animation_NotifyBlue::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_NotifyBlue>);
	ed.name = ANIM_NOTIFY_BLUE;
	return ed;
}

bool Animation_NotifyBlue::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_NotifyBlue::getDefinition());

