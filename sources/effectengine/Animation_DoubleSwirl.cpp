/* Animation_DoubleSwirl.cpp
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

#include <effectengine/Animation_DoubleSwirl.h>

Animation_DoubleSwirl::Animation_DoubleSwirl(QString name) :
	Animation_Swirl(name)
{
	custom_colors.clear();
	custom_colors2.clear();

	center_x = 0.5;
	center_x2 = 0.5;
	center_y = 0.5;
	center_y2 = 0.5;

	custom_colors.append({ 255,0,0 });
	custom_colors.append({ 20,0,255 });


	custom_colors2.append({ 255,0,0,0 });
	custom_colors2.append({ 255,0,0,0 });
	custom_colors2.append({ 255,0,0,0 });
	custom_colors2.append({ 0,0,0,1 });
	custom_colors2.append({ 0,0,0,0 });
	custom_colors2.append({ 255,0,0,0 });
	custom_colors2.append({ 255,0,0,0 });
	custom_colors2.append({ 0,0,0,1 });

	enable_second = true;
	random_center = false;
	random_center2 = false;
	reverse = false;
	reverse2 = true;
	rotation_time = 25;
}

EffectDefinition Animation_DoubleSwirl::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_DOUBLE_SWIRL;
	ed.args = GetArgs();
	return ed;
}

QJsonObject Animation_DoubleSwirl::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}
