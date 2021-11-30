/* Animation_SystemShutdown.cpp
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

#include <effectengine/Animation_SystemShutdown.h>

Animation_SystemShutdown::Animation_SystemShutdown() :
	AnimationBase(ANIM_SYSTEM_SHUTDOWN)
{
	speed = 1.2 * 0.5;
	alarm_color = { 255, 0, 0 };
	post_color = { 255, 174, 11 };
	shutdown_enabled = false;
	initial_blink = true,
		set_post_color = true;
	initial_blink_index = 0;
	yIndex = SYSTEMSHUTDOWN_HEIGHT;
};

EffectDefinition Animation_SystemShutdown::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_SYSTEM_SHUTDOWN;
	ed.args = GetArgs();
	return ed;
}

void Animation_SystemShutdown::Init(
	QImage& hyperImage,
	int hyperLatchTime
)
{

	hyperImage = hyperImage.scaled(SYSTEMSHUTDOWN_WIDTH, SYSTEMSHUTDOWN_HEIGHT);

	SetSleepTime(int(round(speed * 1000.0)));
}

void Animation_SystemShutdown::setLine(QPainter* painter, int y, Point3d rgb)
{
	painter->setPen(QColor().fromRgb(rgb.x, rgb.y, rgb.z));
	painter->drawLine(0, y, SYSTEMSHUTDOWN_WIDTH, y);
}

void Animation_SystemShutdown::setFill(QPainter* painter, Point3d rgb)
{
	painter->fillRect(0, 0, SYSTEMSHUTDOWN_WIDTH, SYSTEMSHUTDOWN_HEIGHT, QColor().fromRgb(rgb.x, rgb.y, rgb.z));
}

bool Animation_SystemShutdown::Play(QPainter* painter)
{
	bool ret = true;

	if (initial_blink)
	{
		if (initial_blink_index < 6)
		{
			if (initial_blink_index++ % 2)
				setFill(painter, alarm_color);
			else
				setFill(painter, Point3d() = { 0,0,0 });
			return ret;
		}
		else
		{
			setFill(painter, Point3d() = { 0,0,0 });
			initial_blink = false;
			return ret;
		}
	}

	if (yIndex-- >= 0)
	{
		setLine(painter, yIndex, alarm_color);
		return ret;
	}

	if (yIndex-- == -1)
	{
		SetSleepTime(int(round(1000.0)));
		return ret;
	}

	if (set_post_color)
	{
		setFill(painter, post_color);
		SetSleepTime(int(round(2000.0)));
		set_post_color = false;
	}
	else
		setStopMe(true);

	return ret;
}

QJsonObject Animation_SystemShutdown::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}





