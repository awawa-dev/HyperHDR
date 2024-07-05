/* Animation_FullColorMoodBlobs.cpp
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

#include <effects/Animation_FullColorMoodBlobs.h>

Animation_FullColorMoodBlobs::Animation_FullColorMoodBlobs() :
	Animation_MoodBlobs()
{
	rotationTime = 60.0;
	colorRandom = true;
	hueChange = 30.0;
	blobs = 5;
	reverse = false;
	baseColorChange = true;
	baseColorRangeLeft = 0;
	baseColorRangeRight = 360;
	baseColorChangeRate = 0.2;
}

EffectDefinition Animation_FullColorMoodBlobs::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_FullColorMoodBlobs>);
	ed.name = ANIM_FULLCOLOR_MOOD_BLOBS;
	return ed;
}

bool Animation_FullColorMoodBlobs::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_FullColorMoodBlobs::getDefinition());
