#include <utils/RgbChannelAdjustment.h>
#include <algorithm>
#include <QJsonArray>

RgbChannelAdjustment::RgbChannelAdjustment(QString channelName)
	: _channelName(channelName)
	, _log(Logger::getInstance(channelName))
{
	resetInitialized();
}


RgbChannelAdjustment::RgbChannelAdjustment(quint8 instance, uint8_t adjustR, uint8_t adjustG, uint8_t adjustB, QString channelName)
	: _channelName(channelName)
	, _log(Logger::getInstance(channelName.replace("ChannelAdjust_", "ADJUST_") + QString::number(instance)))
{
	resetInitialized();
	setAdjustment(adjustR, adjustG, adjustB);
}

void RgbChannelAdjustment::resetInitialized()
{
	_correction = 0;
	_brightness = 0;

	std::fill(std::begin(_adjust), std::end(_adjust), 0);
	std::fill(std::begin(_initialized), std::end(_initialized), false);
	std::fill(std::begin(_mappingAdjR), std::end(_mappingAdjR), 0);
	std::fill(std::begin(_mappingAdjG), std::end(_mappingAdjG), 0);
	std::fill(std::begin(_mappingAdjB), std::end(_mappingAdjB), 0);
	std::fill(std::begin(_mappingCorection), std::end(_mappingCorection), 0);

	memset(&_mapping, 0, sizeof(_mapping));
}

void RgbChannelAdjustment::setAdjustment(uint8_t adjustR, uint8_t adjustG, uint8_t adjustB)
{
	_adjust[RED] = adjustR;
	_adjust[GREEN] = adjustG;
	_adjust[BLUE] = adjustB;

	std::fill(std::begin(_initialized), std::end(_initialized), false);
	initializeAdjustMapping(adjustR, adjustG, adjustB);
}

void RgbChannelAdjustment::setCorrection(uint8_t correction)
{
	if (_correction != correction)
		Info(_log, "Set correction to %d", correction);

	_correction = correction;
	initializeCorrectionMapping(correction);
}
uint8_t RgbChannelAdjustment::getCorrection() const
{
	return _correction;
}
uint8_t RgbChannelAdjustment::correction(uint8_t input) const
{
	return _mappingCorection[input];
}
uint8_t RgbChannelAdjustment::getAdjustmentR() const
{
	return _adjust[RED];
}

uint8_t RgbChannelAdjustment::getAdjustmentG() const
{
	return _adjust[GREEN];
}

uint8_t RgbChannelAdjustment::getAdjustmentB() const
{
	return _adjust[BLUE];
}

uint8_t RgbChannelAdjustment::adjustmentR(uint8_t inputR) const
{
	return _mappingAdjR[inputR];
}

uint8_t RgbChannelAdjustment::adjustmentG(uint8_t inputG) const
{
	return _mappingAdjG[inputG];
}

uint8_t RgbChannelAdjustment::adjustmentB(uint8_t inputB) const
{
	return _mappingAdjB[inputB];
}


void RgbChannelAdjustment::apply(uint8_t input, uint8_t brightness, uint8_t& red, uint8_t& green, uint8_t& blue)
{
	if (_brightness != brightness)
	{
		_brightness = brightness;
		std::fill(std::begin(_initialized), std::end(_initialized), false);
	}

	if (!_initialized[input])
	{
		_mapping[RED][input] = qMin(((_brightness * input * _adjust[RED]) / 65025), (int)UINT8_MAX);
		_mapping[GREEN][input] = qMin(((_brightness * input * _adjust[GREEN]) / 65025), (int)UINT8_MAX);
		_mapping[BLUE][input] = qMin(((_brightness * input * _adjust[BLUE]) / 65025), (int)UINT8_MAX);
		_initialized[input] = true;
	}
	red = _mapping[RED][input];
	green = _mapping[GREEN][input];
	blue = _mapping[BLUE][input];
}

void RgbChannelAdjustment::initializeAdjustMapping(uint8_t _adjustR, uint8_t _adjustG, uint8_t _adjustB)
{
	// initialize the mapping
	for (int i = 0; i < 256; ++i)
	{
		int outputR = (i * _adjustR) / 255;
		if (outputR > 255)
		{
			outputR = 255;
		}
		_mappingAdjR[i] = outputR;
	}
	for (int i = 0; i < 256; ++i)
	{
		int outputG = (i * _adjustG) / 255;
		if (outputG > 255)
		{
			outputG = 255;
		}
		_mappingAdjG[i] = outputG;
	}
	for (int i = 0; i < 256; ++i)
	{
		int outputB = (i * _adjustB) / 255;
		if (outputB > 255)
		{
			outputB = 255;
		}
		_mappingAdjB[i] = outputB;
	}
}


void RgbChannelAdjustment::initializeCorrectionMapping(uint8_t correction)
{
	// initialize the mapping
	for (int i = 0; i < 256; ++i)
	{
		int outputR = (i * correction) / 255;
		if (outputR < -255)
		{
			outputR = -255;
		}
		else if (outputR > 255)
		{
			outputR = 255;
		}
		_mappingCorection[i] = outputR;
	}
}

RgbChannelAdjustment RgbChannelAdjustment::createRgbChannelAdjustment(quint8 instance, const QJsonObject& colorConfig, const QString& channelName, int defaultR, int defaultG, int defaultB)
{
	const QJsonArray& channelConfig = colorConfig[channelName].toArray();

	return RgbChannelAdjustment(
		instance,
		static_cast<uint8_t>(channelConfig[0].toInt(defaultR)),
		static_cast<uint8_t>(channelConfig[1].toInt(defaultG)),
		static_cast<uint8_t>(channelConfig[2].toInt(defaultB)),
		"ChannelAdjust_" + channelName.toUpper()
	);
}



