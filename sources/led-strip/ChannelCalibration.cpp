/* ChannelCalibration.cpp
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
	#include <QJsonArray>
	#include <algorithm>
#endif

#include <led-strip/ChannelCalibration.h>

ChannelCalibration::ChannelCalibration(quint8 instance, QString channelName, const QJsonObject& colorConfig, int defaultR, int defaultG, int defaultB)
	: _channelName(channelName)
	, _log(Logger::getInstance(QString("CHANNEL_%1%2").arg(channelName.toUpper()).arg(instance)))
{
	const QJsonArray& channelConfig = colorConfig[channelName].toArray();
	_targetCalibration.red = static_cast<uint8_t>(channelConfig[0].toInt(defaultR));
	_targetCalibration.green = static_cast<uint8_t>(channelConfig[1].toInt(defaultG));
	_targetCalibration.blue = static_cast<uint8_t>(channelConfig[2].toInt(defaultB));

	_enabled = (_targetCalibration.red != defaultR) || (_targetCalibration.green != defaultG) || (_targetCalibration.blue != defaultB);

	if (!_channelName.compare("red", Qt::CaseInsensitive))
		_correction = colorConfig["temperatureRed"].toInt(255);
	else if (!_channelName.compare("green", Qt::CaseInsensitive))
		_correction = colorConfig["temperatureGreen"].toInt(255);
	else if (!_channelName.compare("blue", Qt::CaseInsensitive))
		_correction = colorConfig["temperatureBlue"].toInt(255);
	else
		_correction = 255;

	if (_enabled || _correction != 255)
	{
		Debug(_log, "Target: [%i, %i, %i, %s], Correction: %i",
			_targetCalibration.red, _targetCalibration.green, _targetCalibration.blue, (_enabled ? "active" : "default"), _correction);
	}
}

void ChannelCalibration::setAdjustment(const QJsonArray& value)
{
	if (value.size() != 3)
		return;
	_targetCalibration.red = static_cast<uint8_t>(value[0].toInt(_targetCalibration.red));
	_targetCalibration.green = static_cast<uint8_t>(value[1].toInt(_targetCalibration.green));
	_targetCalibration.blue = static_cast<uint8_t>(value[2].toInt(_targetCalibration.blue));
	_enabled = true;
	Debug(_log, "setAdjustment: [%i, %i, %i, %s]", _targetCalibration.red, _targetCalibration.green, _targetCalibration.blue, (_enabled ? "active" : "default"));
}

void ChannelCalibration::setCorrection(int correction)
{
	if (correction < 0)
		return;

	_correction = correction;

	Debug(_log, "setCorrection: %i", _correction);
}

QString ChannelCalibration::getChannelName()
{
	return _channelName;
}

uint8_t ChannelCalibration::getCorrection() const
{
	return _correction;
}
uint8_t ChannelCalibration::correction(uint8_t input) const
{
	return (input * _correction) / 255;;
}
ColorRgb ChannelCalibration::getAdjustment() const
{
	return _targetCalibration;
}

uint8_t ChannelCalibration::adjustmentR(uint8_t inputR) const
{
	return (inputR * _targetCalibration.red) / 255;
}

uint8_t ChannelCalibration::adjustmentG(uint8_t inputG) const
{
	return (inputG * _targetCalibration.green) / 255;
}

uint8_t ChannelCalibration::adjustmentB(uint8_t inputB) const
{
	return (inputB * _targetCalibration.blue) / 255;
}

bool ChannelCalibration::isEnabled() const
{
	return _enabled;
}

void ChannelCalibration::apply(uint8_t input, uint8_t brightness, uint8_t& red, uint8_t& green, uint8_t& blue)
{	
	red = std::min(((brightness * input * _targetCalibration.red) / 65025), (int)UINT8_MAX);
	green = std::min(((brightness * input * _targetCalibration.green) / 65025), (int)UINT8_MAX);
	blue = std::min(((brightness * input * _targetCalibration.blue) / 65025), (int)UINT8_MAX);
}
