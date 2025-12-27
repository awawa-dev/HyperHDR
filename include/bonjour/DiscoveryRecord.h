#pragma once

/* DiscoveryRecord.h
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
	#include <QMetaType>
	#include <QString>
#endif

class DiscoveryRecord
{
public:
	enum Service { Unknown = 0, HyperHDR, WLED, PhilipsHue, HomeAssistant, Pico, ESP32_S2, ESP, SerialPort, REFRESH_ALL };

	Service type;
	QString hostName;
	QString address;
	int     port;
	bool	isExists;
	unsigned int ttl;

	DiscoveryRecord();

	static const QString getmDnsHeader(Service service);
	static const QString getName(Service _type);

	const QString getName() const;
	void resetTTL();
	bool expired();


	bool operator==(const DiscoveryRecord& other) const;
	bool operator!=(const DiscoveryRecord& other) const;
};

Q_DECLARE_METATYPE(DiscoveryRecord)

