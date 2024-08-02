/* Animation_WarmMoodBlobs.cpp
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

#include <effects/Animation_WarmMoodBlobs.h>

Animation_WarmMoodBlobs::Animation_WarmMoodBlobs() :
	Animation_MoodBlobs()
{
	rotationTime = 60.0;
	color = { 255,0,0 };
	hueChange = 30.0;
	blobs = 5;
	reverse = false;
	baseColorChange = true;
	baseColorRangeLeft = 333;
	baseColorRangeRight = 151;
	baseColorChangeRate = 2.0;
}

EffectDefinition Animation_WarmMoodBlobs::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_WarmMoodBlobs>, 200, 25);
	ed.name = ANIM_WARM_MOOD_BLOBS;
	return ed;
}

bool Animation_WarmMoodBlobs::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_WarmMoodBlobs::getDefinition());
