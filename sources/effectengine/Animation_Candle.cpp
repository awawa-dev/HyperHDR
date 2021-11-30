/* Animation_Candle.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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

#include <effectengine/Animation_Candle.h>

Animation_Candle::Animation_Candle(QString name) :
	Animation_CandleLight(name)
{
	colorShift = 4;
	color = { 255, 138, 0 };
}

EffectDefinition Animation_Candle::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_CANDLE;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_Candle::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = true;
	doc["smoothing-time_ms"] = 500;
	doc["smoothing-updateFrequency"] = 20.0;
	return doc;
}
