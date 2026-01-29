/* CoreInfiniteEngine.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
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

#include <infinite-color-engine/CoreInfiniteEngine.h>
#include <infinite-color-engine/InfiniteSmoothing.h>
#include <base/HyperHdrInstance.h>

using namespace hyperhdr;
using namespace ColorSpaceMath;
using namespace linalg::aliases;

CoreInfiniteEngine::CoreInfiniteEngine(HyperHdrInstance* hyperhdr, LedString::ColorOrder colorOrder)
	: QObject(),
	_smoothing(std::make_unique<InfiniteSmoothing>(hyperhdr->getSetting(settings::type::SMOOTHING), hyperhdr)),
	_processing(std::make_unique<InfiniteProcessing>(hyperhdr->getSetting(settings::type::COLOR), colorOrder, QString("COLORS%1").arg(hyperhdr->getInstanceIndex()))),
	_log(QString("ENGINE%1").arg(hyperhdr->getInstanceIndex()))
{
	qRegisterMetaType<SharedOutputColors>("SharedOutputColors");
	connect(hyperhdr, &HyperHdrInstance::SignalInstanceSettingsChanged, _smoothing.get(), &InfiniteSmoothing::handleSignalInstanceSettingsChanged);
	connect(hyperhdr, &HyperHdrInstance::SignalRequestComponent, _smoothing.get(), &InfiniteSmoothing::handleSignalRequestComponent);
	connect(hyperhdr, &HyperHdrInstance::SignalSmoothingClockTick, _smoothing.get(), &InfiniteSmoothing::SignalMasterClockTick, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
	connect(hyperhdr, &HyperHdrInstance::SignalInstanceSettingsChanged, _processing.get(), &InfiniteProcessing::handleSignalInstanceSettingsChanged);
	connect(_smoothing.get(), &InfiniteSmoothing::SignalProcessedColors, hyperhdr, &HyperHdrInstance::SignalFinalOutputColorsReady, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
}

int CoreInfiniteEngine::getSuggestedInterval()
{
	return _smoothing->getSuggestedInterval();
}

bool CoreInfiniteEngine::getAntiFlickeringFilterState()
{
	return _smoothing->getAntiFlickeringFilterState();
}

unsigned CoreInfiniteEngine::addCustomSmoothingConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool pause)
{
	return _smoothing->addCustomSmoothingConfig(cfgID, settlingTime_ms, ledUpdateFrequency_hz, pause);
}

void CoreInfiniteEngine::setCurrentSmoothingConfigParams(unsigned cfgID)
{
	_smoothing->setCurrentSmoothingConfigParams(cfgID);
}

void CoreInfiniteEngine::incomingColors(std::vector<float3>&& _ledBuffer)
{
	_processing->applyyAllProcessingSteps(_ledBuffer);
	_smoothing->incomingColors(std::move(_ledBuffer), _processing->getMinimalBacklight());
}

void CoreInfiniteEngine::setProcessingEnabled(bool enabled)
{
	_processing->setProcessingEnabled(enabled);
}

void CoreInfiniteEngine::updateCurrentProcessingConfig(const QJsonObject& config)
{
	_processing->updateCurrentConfig(config);
}

QJsonArray CoreInfiniteEngine::getCurrentProcessingConfig()
{
	return _processing->getCurrentConfig();
}


