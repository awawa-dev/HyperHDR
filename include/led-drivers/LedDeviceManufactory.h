#pragma once

/* LedDeviceManufactory.h
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

#ifndef PCH_ENABLED
	#include <QString>
	#include <QJsonObject>
	#include <list>
	#include <functional>
#endif

class LedDevice;

typedef std::function<LedDevice* (const QJsonObject& deviceConfig)> LedDeviceConstructor;

namespace hyperhdr::leds
{
	struct LedDeviceDefinition
	{
		QString name;
		QString group;
		LedDeviceConstructor constructor;
		LedDeviceDefinition(QString _name, QString _group, LedDeviceConstructor _constructor);
	};

	const std::list<LedDeviceDefinition>& GET_ALL_LED_DEVICE(const LedDeviceDefinition* ed);
	bool REGISTER_LED_DEVICE(QString name, QString group, LedDeviceConstructor constructor);
	LedDevice* CONSTRUCT_LED_DEVICE(const QJsonObject& deviceConfig);
};
