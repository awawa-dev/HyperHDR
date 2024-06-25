#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QList>
	#include <QRectF>

	#include <cstdint>
#endif

#include <image/ColorRgb.h>
#include <image/Image.h>
#include <utils/Logger.h>
#include <utils/Components.h>

#include <utils/FrameDecoder.h>

class Grabber;

class DetectionManual
{
public:
	DetectionManual();

	bool getDetectionManualSignal();

	bool checkSignalDetectionManual(Image<ColorRgb>& image);

	void setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold);

	void setSignalDetectionOffset(double horizontalMin, double verticalMin, double horizontalMax, double verticalMax);

private:
	Logger*		_log;
	double		_x_frac_min;
	double		_y_frac_min;
	double		_x_frac_max;
	double		_y_frac_max;

	int         _noSignalCounterThreshold;
	ColorRgb	_noSignalThresholdColor;

	bool		_noSignalDetected;
	int			_noSignalCounter;
};
