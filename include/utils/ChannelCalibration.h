#pragma once

/* ChannelCalibration.h
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
	#include <QJsonArray>

	#include <cstdint>
#endif

#include <image/ColorRgb.h>
#include <utils/Logger.h>

class ChannelCalibration
{

public:
	ChannelCalibration(quint8 instance, QString channelName, const QJsonObject& colorConfig, int defaultR, int defaultG, int defaultB);

	void apply(uint8_t input, uint8_t brightness, uint8_t& red, uint8_t& green, uint8_t& blue);

	ColorRgb getAdjustment() const;
	void setAdjustment(const QJsonArray& value);

	uint8_t adjustmentR(uint8_t inputR) const;
	uint8_t adjustmentG(uint8_t inputG) const;
	uint8_t adjustmentB(uint8_t inputB) const;

	uint8_t getCorrection() const;
	void setCorrection(int correction);
	uint8_t correction(uint8_t input) const;
	bool isEnabled() const;
	QString getChannelName();

private:
	ColorRgb _targetCalibration;
	int		_correction;
	QString _channelName;
	Logger* _log;
	bool	_enabled;
};
