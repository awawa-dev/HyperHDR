/* Animation_SeaWaves.cpp
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

#include <effects/Animation_SeaWaves.h>

Animation_SeaWaves::Animation_SeaWaves() :
	Animation_Waves()
{
	custom_colors.clear();

	center_x = 1.25;
	center_y = -0.25;

	custom_colors.push_back({ 8,0,255 });
	custom_colors.push_back({ 4,80,255 });
	custom_colors.push_back({ 0,161,255 });
	custom_colors.push_back({ 0,191,255 });
	custom_colors.push_back({ 0,222,255 });
	custom_colors.push_back({ 0,188,255 });
	custom_colors.push_back({ 0,153,255 });
	custom_colors.push_back({ 19,76,255 });
	custom_colors.push_back({ 38,0,255 });
	custom_colors.push_back({ 19,100,255 });
	custom_colors.push_back({ 0,199,255 });

	random_center = false;
	reverse = false;
	reverse_time = 0;
	rotation_time = 60;
}

EffectDefinition Animation_SeaWaves::getDefinition()
{
	EffectDefinition ed(EffectFactory<Animation_SeaWaves>, 200, 25);
	ed.name = ANIM_SEAWAVES;
	return ed;
}

bool Animation_SeaWaves::isRegistered = hyperhdr::effects::REGISTER_EFFECT(Animation_SeaWaves::getDefinition());
