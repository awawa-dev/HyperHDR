#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>

	#include <vector>
#endif

#include <image/ColorRgb.h>
#include <utils/settings.h>
#include <utils/Components.h>
#include <utils/InternalClock.h>
#include <utils/Logger.h>
#include <infinite-color-engine/InfiniteInterpolator.h>
#include <infinite-color-engine/InfiniteProcessing.h>
#include <infinite-color-engine/SharedOutputColors.h>

class HyperHdrInstance;
class InfiniteSmoothing;

class CoreInfiniteEngine : public QObject
{
	Q_OBJECT

public:
	CoreInfiniteEngine(HyperHdrInstance* hyperhdr);
	~CoreInfiniteEngine() = default;

	int getSuggestedInterval();
	unsigned addCustomSmoothingConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, bool pause);
	void setCurrentSmoothingConfigParams(unsigned cfgID);
	void incomingColors(std::vector<linalg::aliases::float3>&& _ledBuffer);
	void setProcessingEnabled(bool enabled);
	void updateCurrentProcessingConfig(const QJsonObject& config);
	QJsonArray getCurrentProcessingConfig();

private:
	std::unique_ptr<InfiniteSmoothing> _smoothing;
	std::unique_ptr<InfiniteProcessing> _processing;
	Logger* _log;
};
