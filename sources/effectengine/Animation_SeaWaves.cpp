/* Animation_SeaWaves.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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

#include <effectengine/Animation_SeaWaves.h>

Animation_SeaWaves::Animation_SeaWaves(QString name) :
	Animation_Waves(name)
{
	custom_colors.clear();

	center_x = 1.25;
	center_y = -0.25;

	custom_colors.append({ 8,0,255 });
	custom_colors.append({ 4,80,255 });
	custom_colors.append({ 0,161,255 });
	custom_colors.append({ 0,191,255 });
	custom_colors.append({ 0,222,255 });
	custom_colors.append({ 0,188,255 });
	custom_colors.append({ 0,153,255 });
	custom_colors.append({ 19,76,255 });
	custom_colors.append({ 38,0,255 });
	custom_colors.append({ 19,100,255 });
	custom_colors.append({ 0,199,255 });

	random_center = false;
	reverse = false;
	reverse_time = 0;
	rotation_time = 60;
}

EffectDefinition Animation_SeaWaves::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_SEAWAVES;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_SeaWaves::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = true;
	doc["smoothing-time_ms"] = 200;
	doc["smoothing-updateFrequency"] = 25.0;
	return doc;
}
