/* AnimationBaseMusic.cpp
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

#include <effects/AnimationBaseMusic.h>

AnimationBaseMusic::AnimationBaseMusic() :
	AnimationBase()
{
	_myTarget.Clear();	
};

AnimationBaseMusic::~AnimationBaseMusic()
{
}

bool AnimationBaseMusic::isSoundEffect()
{
	return true;
};

void AnimationBaseMusic::store(MovingTarget* source) {
	_myTarget.CopyFrom(source);
};

void AnimationBaseMusic::restore(MovingTarget* target) {
	target->CopyFrom(&_myTarget);
};

void AnimationBaseMusic::Init(HyperImage& hyperImage, int hyperLatchTime)
{
	SetSleepTime(17);
};

void MovingTarget::Clear()
{
	_averageColor = ColorRgb(0, 0, 0);
	_fastColor = ColorRgb(0, 0, 0);
	_slowColor = ColorRgb(0, 0, 0);
	_targetAverageR = 0;
	_targetAverageG = 0;
	_targetAverageB = 0;
	_targetAverageCounter = 0;
	_targetSlowR = 0;
	_targetSlowG = 0;
	_targetSlowB = 0;
	_targetSlowCounter = 0;
	_targetFastR = 0;
	_targetFastG = 0;
	_targetFastB = 0;
	_targetFastCounter = 0;
};

void MovingTarget::CopyFrom(MovingTarget* source)
{
	_averageColor = source->_averageColor;
	_fastColor = source->_fastColor;
	_slowColor = source->_slowColor;
	_targetAverageR = source->_targetAverageR;
	_targetAverageG = source->_targetAverageG;
	_targetAverageB = source->_targetAverageB;
	_targetAverageCounter = source->_targetAverageCounter;
	_targetSlowR = source->_targetSlowR;
	_targetSlowG = source->_targetSlowG;
	_targetSlowB = source->_targetSlowB;
	_targetSlowCounter = source->_targetSlowCounter;
	_targetFastR = source->_targetFastR;
	_targetFastG = source->_targetFastG;
	_targetFastB = source->_targetFastB;
	_targetFastCounter = source->_targetFastCounter;
};


