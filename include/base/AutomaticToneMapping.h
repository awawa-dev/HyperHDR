/* AutomaticToneMapping.h
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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

#pragma once

#ifndef PCH_ENABLED
#endif

#include <utils/Logger.h>

class AutomaticToneMapping
{	
public:
	struct ToneMappingThresholds;

	AutomaticToneMapping();

	AutomaticToneMapping* prepare();
	void finilize();
	void setConfig(bool enabled, const ToneMappingThresholds& newConfig, int timeInSec, int timeToDisableInMSec);
	void setToneMapping(bool enabled);

	constexpr uint8_t checkY(uint8_t y)
	{
		if (y > _running.y) _running.y = y;
		return y;
	}

	constexpr uint8_t checkU(uint8_t u)
	{
		if (u > _running.u) _running.u = u;
		return u;
	}

	constexpr uint8_t checkV(uint8_t v)
	{
		if (v > _running.v) _running.v = v;
		return v;
	}

	struct ToneMappingThresholds {
		uint8_t y;
		uint8_t u;
		uint8_t v;
	};

private:
	bool _enabled;
	int _timeInSec;
	int _timeToDisableInMSec;

	ToneMappingThresholds _config, _running;

	bool _modeSDR;
	long _startedTime;
	long _endingTime;
	Logger* _log;

};
