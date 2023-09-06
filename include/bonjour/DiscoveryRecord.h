#pragma once

/* DiscoveryRecord.h
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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

#include <QMetaType>
#include <QString>

class DiscoveryRecord
{
public:
	enum Service { Unknown = 0, HyperHDR, WLED, PhilipsHue, Pico, ESP32_S2, ESP, SerialPort };

	static const QString getmDnsHeader(Service service)
	{
		switch (service)
		{
			case(Service::PhilipsHue): return QLatin1String("_hue._tcp"); break;
			case(Service::WLED): return QLatin1String("_wled._tcp"); break;
			case(Service::HyperHDR): return QLatin1String("_hyperhdr-http._tcp"); break;
			default: return "SERVICE_UNKNOWN";
		}
	}

	const QString getName() const
	{
		return getName(type);
	}

	static const QString getName(Service _type)
	{
		switch (_type)
		{
			case(Service::PhilipsHue): return "PhilipsHue"; break;
			case(Service::WLED): return "WLED"; break;
			case(Service::HyperHDR): return "HyperHDR"; break;
			case(Service::Pico): return "Pico/RP2040"; break;
			case(Service::ESP32_S2): return "ESP32-S2"; break;
			case(Service::ESP): return "ESP board"; break;
			default: return "SERVICE_UNKNOWN";
		}
	}

	void resetTTL()
	{
		ttl = 7;
	}

	bool expired()
	{
		ttl >>= 1;
		if (type == Service::WLED)
			return ttl == 0;
		else if (type == Service::Pico || type == Service::ESP32_S2 || type == Service::ESP)
			return ttl <= 2;
		else
			return ttl <= 1;
	}

	DiscoveryRecord() : type(Service::Unknown), port(-1), isExists(false)
	{
		resetTTL();
	}

	Service type;
	QString hostName;
	QString address;
	int     port;
	bool	isExists;
	unsigned int ttl;
	

	bool operator==(const DiscoveryRecord& other) const
	{
		return type == other.type
			&& address == other.address
			&& port == other.port;
	}

	bool operator!=(const DiscoveryRecord& other) const
	{
		return type != other.type
			|| address != other.address
			|| port != other.port;
	}
};

Q_DECLARE_METATYPE(DiscoveryRecord)

