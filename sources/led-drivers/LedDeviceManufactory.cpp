/* LedDeviceManufactury.cpp
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
	#include <mutex>
#endif

#include <led-drivers/LedDeviceManufactory.h>
#include <utils/Logger.h>
#include <led-drivers/LedDevice.h>
#include <led-drivers/other/DriverOtherFile.h>

namespace hyperhdr::leds
{
	LedDeviceDefinition::LedDeviceDefinition(QString _name, QString _group, LedDeviceConstructor _constructor) :
		name(_name),
		group(_group),
		constructor(_constructor)
	{
	};

	const std::list<LedDeviceDefinition>& GET_ALL_LED_DEVICE(const LedDeviceDefinition* ed)
	{
		static std::list<LedDeviceDefinition> list;

		if (ed != nullptr)
		{
			static std::mutex effectLocker;
			const std::lock_guard<std::mutex> lock(effectLocker);
			list.push_back(*ed);
		}

		return list;
	};

	bool REGISTER_LED_DEVICE(QString name, QString group, LedDeviceConstructor constructor)
	{
		LedDeviceDefinition newLed(name, group, constructor);
		GET_ALL_LED_DEVICE(&newLed);
		return true;
	};

	LedDevice* CONSTRUCT_LED_DEVICE(const QJsonObject& deviceConfig)
	{
		const std::list<LedDeviceDefinition>& drivers = GET_ALL_LED_DEVICE(nullptr);

		Logger* log = Logger::getInstance("LEDDEVICE");
		QJsonDocument config(deviceConfig);

		QString type = deviceConfig["type"].toString("UNSPECIFIED").toLower();

		auto findIter = std::find_if(drivers.begin(), drivers.end(),
			[&type](const LedDeviceDefinition& a)
			{
				if (a.name == type)
					return true;
				else
					return false;
			}
		);

		LedDevice* device = nullptr;

		if (findIter != drivers.end())
			device = (*findIter).constructor(deviceConfig);
		else
		{
			QString dummyDeviceType = "file";
			Error(log, "Dummy device type (%s) used, because configured driver '%s' could not be found", QSTRING_CSTR(dummyDeviceType), QSTRING_CSTR(type));

			QJsonObject dummyDeviceConfig;
			dummyDeviceConfig.insert("type", dummyDeviceType);
			device = DriverOtherFile::construct(dummyDeviceConfig);
		}

		return device;
	};
};
