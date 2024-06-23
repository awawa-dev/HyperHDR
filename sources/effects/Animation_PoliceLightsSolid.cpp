/* Animation_PoliceLightsSolid.cpp
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

#include <effects/Animation_PoliceLightsSolid.h>

Animation_PoliceLightsSolid::Animation_PoliceLightsSolid() :
	Animation_Police()
{
	rotationTime = 1.0;
	colorOne = { 255, 0, 0 };
	colorTwo = { 0, 0, 255 };
	reverse = false;
}

EffectDefinition Animation_PoliceLightsSolid::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_PoliceLightsSolid>);
	ed.name = ANIM_POLICELIGHTSSOLID;
	return ed;
}

bool Animation_PoliceLightsSolid::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_PoliceLightsSolid::getDefinition());

