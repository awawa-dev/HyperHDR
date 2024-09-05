/* LedCalibration.cpp
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
	#include <QJsonValue>
	#include <QRegularExpression>
	#include <QJsonArray>
	#include <sstream>
	#include <algorithm>

	#include <utils/Logger.h>
#endif

#include <led-strip/LedCalibration.h>

LedCalibration::LedCalibration(quint8 instance, int ledNumber, const QJsonObject& colorConfig)
	: _perLedConfig(ledNumber, nullptr)
	, _log(Logger::getInstance("LED_CALIBRATION" + QString::number(instance)))
{
	int index = 0;
	const QRegularExpression overallExp("^([0-9]+(\\-[0-9]+)?)(,[ ]*([0-9]+(\\-[0-9]+)?))*$");
	const QJsonArray& adjustmentConfigArray = colorConfig["channelAdjustment"].toArray();

	for (auto item = adjustmentConfigArray.begin(); item != adjustmentConfigArray.end(); ++item, ++index)
	{
		const QJsonObject& config = (*item).toObject();
		const QString range = config["leds"].toString("").trimmed();
		std::shared_ptr<ColorCalibration> colorAdjustment = std::make_shared<ColorCalibration>(instance, config);

		_calibrationConfig.push_back(colorAdjustment);		

		if (range.compare("*") == 0)
		{
			setAdjustmentForLed(index, colorAdjustment, 0, ledNumber - 1);
			continue;
		}

		if (!overallExp.match(range).hasMatch())
		{
			Warning(_log, "Unrecognized segment range configuration: %s", QSTRING_CSTR(range));
			continue;
		}

		for (const auto& segment : range.split(","))
		{
			int first = 0, second = 0;
			bool ok = false;
			if (segment.contains("-"))
			{
				QStringList ledIndices = segment.split("-");
				if (ledIndices.size() == 2)
				{
					first = ledIndices[0].toInt(&ok);
					second = (ok) ? ledIndices[1].toInt(&ok) : 0;
				}
			}
			else
			{
				first = second = segment.toInt(&ok);
			}

			if (ok)
				setAdjustmentForLed(index, colorAdjustment, first, second);
			else
				Warning(_log, "Unrecognized segment range configuration: %s", QSTRING_CSTR(segment));
		}
	}
}

LedCalibration::~LedCalibration()
{
	Debug(_log, "LedCalibration is released");
}

void LedCalibration::setAdjustmentForLed(int index, std::shared_ptr<ColorCalibration>& adjustment, size_t startLed, size_t endLed)
{
	Debug(_log, "Calibration config '%i' for LED segment: [%d, %d]", index, startLed, endLed);

	for (size_t iLed = startLed; iLed <= endLed; ++iLed)
		if (iLed < 0 || iLed >= _perLedConfig.size())
			Error(_log, "Cannot apply calibration config because LED index '%i' does not exist", iLed);
		else	
			_perLedConfig[iLed] = adjustment;
}

bool LedCalibration::verifyAdjustments() const
{
	bool ok = true;

	for (auto adjustment = _perLedConfig.begin(); adjustment != _perLedConfig.end(); ++adjustment)
		if (*adjustment == nullptr)
		{
			Warning(_log, "No calibration set for led at index: %i", adjustment - _perLedConfig.begin());
			ok = false;
		}

	return ok;
}

void LedCalibration::setBacklightEnabled(bool enable)
{
	for (auto&& adjustment : _calibrationConfig)
	{
		adjustment->setBackLightEnabled(enable);
	}
}

void LedCalibration::applyAdjustment(std::vector<ColorRgb>& ledColors)
{
	const size_t totalNumber = std::min(_perLedConfig.size(), ledColors.size());
	for (size_t i = 0; i < totalNumber; ++i)
	{
		std::shared_ptr<ColorCalibration>& adjustment = _perLedConfig[i];
		
		if (adjustment != nullptr)
		{
			_perLedConfig[i]->calibrate(ledColors[i]);
		}
	}
}

QJsonArray LedCalibration::getAdjustmentState() const
{
	QJsonArray adjustmentArray;

	for (auto&& colorAdjustment: _calibrationConfig)
	{
		QJsonObject adjustment;
		colorAdjustment->putCurrentConfig(adjustment);
		adjustmentArray.append(adjustment);
	}

	return adjustmentArray;
}

void LedCalibration::updateConfig(const QJsonObject& adjustment)
{
	if (_calibrationConfig.size() > 0)
		_calibrationConfig.front()->updateConfig(adjustment);
}

