/* AnimationBase.cpp
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

#include <effects/AnimationBase.h>

AnimationBase::AnimationBase() :
	_sleepTime(100),
	_stopMe(false)
{
};

bool AnimationBase::isStop()
{
	return _stopMe;
}

void AnimationBase::setStopMe(bool stopMe)
{
	_stopMe = stopMe;
}

bool AnimationBase::isSoundEffect()
{
	return false;
};

bool AnimationBase::hasOwnImage()
{
	return false;
};

bool AnimationBase::getImage(Image<ColorRgb>& image)
{
	return false;
};

void AnimationBase::SetSleepTime(int sleepTime)
{
	_sleepTime = sleepTime;
};

int  AnimationBase::GetSleepTime()
{
	return _sleepTime;
};

int AnimationBase::clamp(int v, int lo, int hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

bool AnimationBase::hasLedData(std::vector<ColorRgb>& buffer)
{
	return false;
}

