#pragma once

/* LedCalibration.h
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
	#include <QStringList>
	#include <QString>
	#include <QJsonObject>

	#include <vector>	
#endif

#include <image/ColorRgb.h>
#include <led-strip/ColorCalibration.h>

class LedCalibration
{
public:
	LedCalibration(quint8 instance, int ledCnt, const QJsonObject& colorConfig);
	~LedCalibration();

	LedCalibration(const LedCalibration& t) = delete;
	LedCalibration& operator=(const LedCalibration& x) = delete;
	
	void setAdjustmentForLed(int index, std::shared_ptr<ColorCalibration>& adjustment, size_t startLed, size_t endLed);
	bool verifyAdjustments() const;
	void setBacklightEnabled(bool enable);
	QJsonArray getAdjustmentState() const;
	void applyAdjustment(std::vector<ColorRgb>& ledColors);
	void updateConfig(const QJsonObject& adjustment);

private:
	std::vector<std::shared_ptr<ColorCalibration>> _calibrationConfig;
	std::vector<std::shared_ptr<ColorCalibration>> _perLedConfig;
	Logger* _log;
};
