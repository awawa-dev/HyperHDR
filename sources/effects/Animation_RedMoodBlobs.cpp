/* Animation_RedMoodBlobs.cpp
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

#include <effects/Animation_RedMoodBlobs.h>

Animation_RedMoodBlobs::Animation_RedMoodBlobs() :
	Animation_MoodBlobs()
{
	rotationTime = 60.0;
	color = { 255,0,0 };
	hueChange = 60.0;
	blobs = 5;
	reverse = false;
}

EffectDefinition Animation_RedMoodBlobs::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_RedMoodBlobs>);
	ed.name = ANIM_RED_MOOD_BLOBS;
	return ed;
}

bool Animation_RedMoodBlobs::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_RedMoodBlobs::getDefinition());
