/* Animation_StrobeRed.cpp
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

#include <effects/Animation_StrobeRed.h>

Animation_StrobeRed::Animation_StrobeRed() :
	Animation_Fade()
{
	colorStart = { 255, 0, 0 };
	colorEnd = { 0, 0, 0 };
	fadeInTime = 125;
	fadeOutTime = 125;
	colorStartTime = 125;
	colorEndTime = 125;
	repeat = 0;
	maintainEndCol = true;
}

EffectDefinition Animation_StrobeRed::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_StrobeRed>);
	ed.name = ANIM_STROBE_RED;
	return ed;
}

bool Animation_StrobeRed::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_StrobeRed::getDefinition());
