/* AutomaticToneMapping.cpp
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
#endif

#include <utils/GlobalSignals.h>
#include <utils/InternalClock.h>
#include <base/AutomaticToneMapping.h>

#define DEFAULT_GRACEFUL_TIMEOUT 10

AutomaticToneMapping::AutomaticToneMapping() :
	_enabled(false),
	_timeInSec(30),
	_config{},
	_running{},
	_modeSDR(true),
	_startedTime(0),
	_gracefulTimeout(DEFAULT_GRACEFUL_TIMEOUT),
	_log(Logger::getInstance(QString("AUTOTONEMAPPING")))
{
}

AutomaticToneMapping* AutomaticToneMapping::prepare()
{
	if (_enabled)
	{		
		return this;
	}
	else
	{
		return nullptr;
	}
}

void AutomaticToneMapping::finilize()
{
	if (_enabled)
	{
		bool triggered = (_config.y != _running.y || _config.u != _running.u || _config.v != _running.v);

		if (triggered && !_modeSDR && _gracefulTimeout-- <= 0)
		{
			QString message = "Tone mapping OFF triggered by: ";
			if (_config.y != _running.y)
				message += QString(" Y threshold (%1), ").arg(_running.y);
			if (_config.u != _running.u)
				message += QString(" U threshold (%1), ").arg(_running.u);
			if (_config.v != _running.v)
				message += QString(" V threshold (%1), ").arg(_running.v);
			Info(_log, "%s", QSTRING_CSTR(message));

			_modeSDR = true;
			_startedTime = 0;
			GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, false);
		}
		else if (!triggered && _modeSDR)
		{
			auto now = InternalClock::now() / 1000;
			if (_startedTime == 0 || _startedTime > now)
			{
				_startedTime = now;
			}
			else if (_startedTime + _timeInSec < now)
			{
				_modeSDR = false;
				Info(_log, "Tone mapping ON triggered by configured time");
				GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);
			}
		}

		if (!triggered)
			_gracefulTimeout = DEFAULT_GRACEFUL_TIMEOUT;

		_running = _config;
	}
}

void AutomaticToneMapping::setConfig(bool enabled, const ToneMappingThresholds& newConfig, int timeInSec)
{
	_enabled = enabled;
	_config = newConfig;
	_timeInSec = timeInSec;
	_running = _config;
	Info(_log, "Enabled: %s, Time: %i, Thresholds: %i, %i, %i", (enabled) ? "yes" : "no", timeInSec, _config.y, _config.u, _config.v);
}

void AutomaticToneMapping::setToneMapping(bool enabled)
{
	_modeSDR = !enabled;
	_startedTime = 0;
	_gracefulTimeout = DEFAULT_GRACEFUL_TIMEOUT;
}


