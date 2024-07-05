/* GlobalSignals.h
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

#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
#endif

#include <image/ColorRgb.h>
#include <image/Image.h>
#include <utils/Components.h>

#include <performance-counters/PerformanceCounters.h>
#include <bonjour/DiscoveryRecord.h>

class HyperHdrManager;
class AccessManager;
class SoundCapture;
class GrabberHelper;
class DiscoveryWrapper;

class GlobalSignals : public QObject
{
	Q_OBJECT
public:
	static GlobalSignals* getInstance()
	{
		static GlobalSignals instance;
		return &instance;
	}

private:
	GlobalSignals() = default;

public:
	GlobalSignals(GlobalSignals const&) = delete;
	void operator=(GlobalSignals const&) = delete;

signals:
	void SignalGetInstanceManager(std::shared_ptr<HyperHdrManager>& instanceManager);

	void SignalGetAccessManager(std::shared_ptr<AccessManager>& accessManager);

	void SignalGetSoundCapture(std::shared_ptr<SoundCapture>& soundCapture);

	void SignalGetSystemGrabber(std::shared_ptr<GrabberHelper>& systemGrabber);

	void SignalGetVideoGrabber(std::shared_ptr<GrabberHelper>& videoGrabber);

	void SignalGetPerformanceCounters(std::shared_ptr<PerformanceCounters>& performanceCounters);

	void SignalGetDiscoveryWrapper(std::shared_ptr<DiscoveryWrapper>& discoveryWrapper);

	void SignalNewSystemImage(const QString& name, const Image<ColorRgb>& image);

	void SignalNewVideoImage(const QString& name, const Image<ColorRgb>& image);

	void SignalClearGlobalInput(int priority, bool forceClearAll);

	void SignalSetGlobalImage(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin, QString clientDescription);

	void SignalSetGlobalColor(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, hyperhdr::Components origin, QString clientDescription);

	void SignalRequestComponent(hyperhdr::Components component, int hyperHdrInd, bool listen);

	void SignalPerformanceNewReport(PerformanceReport pr);

	void SignalPerformanceStateChanged(bool enabled, hyperhdr::PerformanceReportType type, int id, QString name = "");

	void SignalDiscoveryRequestToScan(DiscoveryRecord::Service type);

	void SignalDiscoveryEvent(DiscoveryRecord message);	
};
