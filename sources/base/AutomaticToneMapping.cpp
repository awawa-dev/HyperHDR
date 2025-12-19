/* AutomaticToneMapping.cpp
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

#ifndef PCH_ENABLED
#endif

#include <utils/GlobalSignals.h>
#include <utils/InternalClock.h>
#include <base/AutomaticToneMapping.h>
#include <utils/Logger.h>

#define DEFAULT_GRACEFUL_TIMEOUT 10

AutomaticToneMapping::AutomaticToneMapping() :
	_enabled(false),
	_timeInSec(30),
	_timeToDisableInMSec(500),
	_config{},
	_running{},
	_triggered(false),
	_modeSDR(true),
	_startedTime(0),
	_endingTime(0),
	_log("AUTOTONEMAPPING")
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

void AutomaticToneMapping::scan_YUYV(int width, uint8_t* currentSourceY)
{
	auto checkRange = [&]() -> bool {
		const uint32_t limYlo = _running.y;       // 0x00'00'00'FF
		const uint32_t limYhi = _running.y << 16; // 0x00'FF'00'00
		const uint32_t limU = _running.u << 8;    // 0x00'00'FF'00
		const uint32_t limV = _running.v << 24;   // 0xFF'00'00'00
		for (const uint8_t* end = currentSourceY + width * 2; currentSourceY < end; currentSourceY += 4) {
			if (uint32_t yuyv = *reinterpret_cast<uint32_t*>(currentSourceY); (yuyv & 0x00'00'00'FF) > limYlo || (yuyv & 0x00'FF'00'00) > limYhi ||
																			  (yuyv & 0x00'00'FF'00) > limU   || (yuyv & 0xFF'00'00'00) > limV)
			{
				_running.y = std::max(std::max(currentSourceY[0], currentSourceY[2]), _running.y);
				_running.u = std::max(currentSourceY[1], _running.u);
				_running.v = std::max(currentSourceY[3], _running.v);
				return true;
			}
		}
		return false;
	};

	_triggered = _triggered || checkRange();
};

void AutomaticToneMapping::scan_Y_UV_8(int width, uint8_t* currentSourceY, uint8_t* currentSourceUV)
{
	auto checkRange = [&]() -> bool {
		const uint32_t limYlo = _running.y;       // 0x00'00'00'FF
		const uint32_t limYhi = _running.y << 16; // 0x00'FF'00'00
		const uint32_t limUlo = _running.u;       // 0x00'00'00'FF
		const uint32_t limUhi = _running.u << 16; // 0x00'FF'00'00
		const uint32_t limVlo = _running.v << 8;  // 0x00'00'FF'00
		const uint32_t limVhi = _running.v << 24; // 0xFF'00'00'00

		for (const uint8_t* end = currentSourceY + width;   currentSourceY < end; currentSourceY += 4)
			if (uint32_t y = *reinterpret_cast<uint32_t*>(currentSourceY); (y & 0x00'00'00'FF) > limYlo || (y & 0x00'FF'00'00) > limYhi)
			{
				_running.y = std::max(currentSourceY[0], currentSourceY[2]);
				return true;
			}

		for (const uint8_t* end = currentSourceUV + width / 2; currentSourceUV < end; currentSourceUV += 4)
			if (uint32_t uv = *reinterpret_cast<uint32_t*>(currentSourceUV); (uv & 0x00'00'00'FF) > limUlo || (uv & 0x00'FF'00'00) > limUhi ||
																			 (uv & 0x00'00'FF'00) > limVlo || (uv & 0xFF'00'00'00) > limVhi)
			{
				_running.u = std::max(std::max(currentSourceUV[0], currentSourceUV[2]), _running.u);
				_running.v = std::max(std::max(currentSourceUV[1], currentSourceUV[3]), _running.v);
				return true;
			}
		return false;
	};

	_triggered = _triggered || checkRange();
}

void AutomaticToneMapping::scan_Y_UV_16(int width, uint8_t* currentSourceY, uint8_t* currentSourceUV)
{
	auto checkRange = [&]() -> bool {
		const uint32_t limYlo = _running.y << 8;  // 0x00'00'FF'00
		const uint32_t limYhi = _running.y << 24; // 0xFF'00'00'00
		const uint32_t limU = _running.u << 8;    // 0x00'00'FF'00
		const uint32_t limV = _running.v << 24;   // 0xFF'00'00'00

		for (const uint8_t* end = currentSourceY + 2 * width; currentSourceY < end; currentSourceY += 4)
			if (uint32_t y = *reinterpret_cast<uint32_t*>(currentSourceY); (y & 0x00'00'FF'00) > limYlo || (y & 0xFF'00'00'00) > limYhi)
			{
				_running.y = std::max(currentSourceY[1], currentSourceY[3]);
				return true;
			}

		for (const uint8_t* end = currentSourceUV + width; currentSourceUV < end; currentSourceUV += 4)
			if (uint32_t uv = *reinterpret_cast<uint32_t*>(currentSourceUV); (uv & 0x00'00'FF'00) > limU || (uv & 0xFF'00'00'00) > limV)
			{
				_running.u = std::max(currentSourceUV[1], _running.u);
				_running.v = std::max(currentSourceUV[3], _running.v);
				return true;
			}
		return false;
	};

	_triggered = _triggered || checkRange();
}

void AutomaticToneMapping::finilize()
{
	if (_enabled)
	{
		bool triggered = (_config.y != _running.y || _config.u != _running.u || _config.v != _running.v) || _triggered;

		if (triggered && !_modeSDR)
		{
			auto now = InternalClock::now();
			if (_endingTime == 0 || _endingTime > now)
			{
				_endingTime = now;
			}
			else if (_endingTime + _timeToDisableInMSec <= now)
			{
				QString message = "Tone mapping OFF triggered by: ";
				if (_config.y != _running.y)
					message += QString(" Y threshold (%1), ").arg(_running.y);
				if (_config.u != _running.u)
					message += QString(" U threshold (%1), ").arg(_running.u);
				if (_config.v != _running.v)
					message += QString(" V threshold (%1), ").arg(_running.v);
				if (_triggered)
					message += QString(" trigger flag, ");
				Info(_log, "{:s} after {:d} ms", (message), now - _endingTime);


				_modeSDR = true;
				emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, false);
			}
		}
		else if (!triggered && _modeSDR)
		{
			auto now = InternalClock::now() / 1000;
			if (_startedTime == 0 || _startedTime > now)
			{
				_startedTime = now;
			}
			else if (_startedTime + _timeInSec <= now)
			{
				_modeSDR = false;
				Info(_log, "Tone mapping ON triggered after {:d} sec", now - _startedTime);
				emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);
			}
		}

		if (!triggered)
		{
			_endingTime = 0;
		}
		else
		{
			_startedTime = 0;
		}

		_running = _config;
		_triggered = false;
	}
}

void AutomaticToneMapping::setConfig(bool enabled, const ToneMappingThresholds& newConfig, int timeInSec, int timeToDisableInMSec)
{
	_enabled = enabled;
	_config = newConfig;
	_timeInSec = timeInSec;
	_timeToDisableInMSec = timeToDisableInMSec;
	_running = _config;
	_triggered = false;
	Info(_log, "Enabled: {:s}, time to enable: {:d}s, time to disable: {:d}ms, thresholds: {{ {:d}, {:d}, {:d}}}", (enabled) ? "yes" : "no", _timeInSec, _timeToDisableInMSec, _config.y, _config.u, _config.v);
}

void AutomaticToneMapping::setToneMapping(bool enabled)
{
	Info(_log, "Tone mapping is currently: {:s}", (enabled) ? "enabled" : "disabled");
	_modeSDR = !enabled;
	_startedTime = 0;
	_endingTime = 0;
}


