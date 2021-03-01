#include <utils/RgbChannelAdjustment.h>

RgbChannelAdjustment::RgbChannelAdjustment(QString channelName)
	: _channelName(channelName)
	, _log(Logger::getInstance(channelName))
	, _brightness(0)
{
	resetInitialized();
}

RgbChannelAdjustment::RgbChannelAdjustment(uint8_t adjustR, uint8_t adjustG, uint8_t adjustB, QString channelName)
	: _channelName(channelName)
	, _log(Logger::getInstance(channelName))
	, _brightness(0)
{
	setAdjustment(adjustR, adjustG, adjustB);
}

void RgbChannelAdjustment::resetInitialized()
{
	//Debug(_log, "initialize mapping with %d,%d,%d", _adjust[RED], _adjust[GREEN], _adjust[BLUE]);
	memset(_initialized, false, sizeof(_initialized));
}

void RgbChannelAdjustment::setAdjustment(uint8_t adjustR, uint8_t adjustG, uint8_t adjustB)
{
	_adjust[RED]   = adjustR;
	_adjust[GREEN] = adjustG;
	_adjust[BLUE]  = adjustB;
	resetInitialized();
	initializeAdjustMapping(adjustR, adjustG, adjustB);	
}

void RgbChannelAdjustment::setCorrection(uint8_t correction)
{
	_correction=correction;
	initializeCorrectionMapping(correction);	
	Debug(_log, "set color correction to %d", _correction);	
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


void RgbChannelAdjustment::apply(uint8_t input, uint8_t brightness, uint8_t & red, uint8_t & green, uint8_t & blue)
{
	if (_brightness != brightness)
	{
		_brightness = brightness;
		resetInitialized();
	}

	if (!_initialized[input])
	{
		_mapping[RED  ][input] = qMin( ((_brightness * input * _adjust[RED  ]) / 65025), (int)UINT8_MAX);
		_mapping[GREEN][input] = qMin( ((_brightness * input * _adjust[GREEN]) / 65025), (int)UINT8_MAX);
		_mapping[BLUE ][input] = qMin( ((_brightness * input * _adjust[BLUE ]) / 65025), (int)UINT8_MAX);
		_initialized[input] = true;
	}
	red   = _mapping[RED  ][input];
	green = _mapping[GREEN][input];
	blue  = _mapping[BLUE ][input];
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

