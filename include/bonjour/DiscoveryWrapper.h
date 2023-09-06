#pragma once

/* DiscoveryWrapper.h
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

#include <QObject>
#include <QList>
#include <QHostInfo>

#include <bonjour/DiscoveryRecord.h>
#include <utils/Logger.h>
#include <leddevice/LedDevice.h>

class DiscoveryWrapper : public QObject
{
	Q_OBJECT
private:
	friend class HyperHdrDaemon;

	Logger*		_log;
	LedDevice*	_serialDevice;

	DiscoveryWrapper(QObject* parent = nullptr);

public:
	~DiscoveryWrapper();

	static DiscoveryWrapper* instance;
	static DiscoveryWrapper* getInstance() { return instance; }

public slots:
	QList<DiscoveryRecord> getPhilipsHUE();
	QList<DiscoveryRecord> getWLED();
	QList<DiscoveryRecord> getHyperHDRServices();
	QList<DiscoveryRecord> getAllServices();
	void requestServicesScan();

	void discoveryEventHandler(DiscoveryRecord message);
	void requestToScanHandler(DiscoveryRecord::Service type);

signals:
	void foundService(DiscoveryRecord::Service type, QList<DiscoveryRecord> records);
	void discoveryEvent(DiscoveryRecord message);
	void requestToScan(DiscoveryRecord::Service type);

private:
	void gotMessage(QList<DiscoveryRecord>& target, DiscoveryRecord message);
	void cleanUp(QList<DiscoveryRecord>& target);

	// contains all current active service sessions
	QList<DiscoveryRecord> _hyperhdrSessions, _wledDevices, _hueDevices, _espDevices, _picoDevices, _esp32s2Devices;
};
