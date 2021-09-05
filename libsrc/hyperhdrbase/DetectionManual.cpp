
#include <hyperhdrbase/Grabber.h>

DetectionManual::DetectionManual():
	_log(Logger::getInstance("SIGNAL_OLD"))
	, _x_frac_min(0.25)
	, _y_frac_min(0.25)
	, _x_frac_max(0.75)
	, _y_frac_max(0.75)
	, _noSignalCounterThreshold(40)
	, _noSignalThresholdColor(ColorRgb{ 0,0,0 })
	, _noSignalDetected(false)
	, _noSignalCounter(0)
{

};

bool DetectionManual::checkSignalDetectionManual(Image<ColorRgb>& image)
{	
	// check signal (only in center of the resulting image, because some grabbers have noise values along the borders)
	bool noSignal = true;

	// top left
	unsigned xOffset = image.width() * _x_frac_min;
	unsigned yOffset = image.height() * _y_frac_min;

	// bottom right
	unsigned xMax = image.width() * _x_frac_max;
	unsigned yMax = image.height() * _y_frac_max;


	for (unsigned x = xOffset; noSignal && x < xMax; ++x)
	{
		for (unsigned y = yOffset; noSignal && y < yMax; ++y)
		{
			noSignal &= (ColorRgb&)image(x, y) <= _noSignalThresholdColor;
		}
	}

	if (noSignal)
	{
		++_noSignalCounter;
	}
	else
	{
		if (_noSignalCounter >= _noSignalCounterThreshold)
		{
			_noSignalDetected = true;
			Info(_log, "Signal detected");
		}

		_noSignalCounter = 0;
	}

	if (_noSignalCounter < _noSignalCounterThreshold)
	{
		return true;
	}
	else if (_noSignalCounter == _noSignalCounterThreshold)
	{
		_noSignalDetected = false;
		Info(_log, "Signal lost");
	}

	return false;
}

void DetectionManual::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	_noSignalThresholdColor.red = uint8_t(255 * redSignalThreshold);
	_noSignalThresholdColor.green = uint8_t(255 * greenSignalThreshold);
	_noSignalThresholdColor.blue = uint8_t(255 * blueSignalThreshold);
	_noSignalCounterThreshold = qMax(1, noSignalCounterThreshold);
	
	Debug(_log, "Signal threshold set to: {%d, %d, %d} and frames: %d", _noSignalThresholdColor.red, _noSignalThresholdColor.green, _noSignalThresholdColor.blue, _noSignalCounterThreshold);
}

void DetectionManual::setSignalDetectionOffset(double horizontalMin, double verticalMin, double horizontalMax, double verticalMax)
{
	_x_frac_min = horizontalMin;
	_y_frac_min = verticalMin;
	_x_frac_max = horizontalMax;
	_y_frac_max = verticalMax;
	
	Debug(_log, "Signal detection area set to: %f,%f x %f,%f", _x_frac_min, _y_frac_min, _x_frac_max, _y_frac_max);
}

bool DetectionManual::getDetectionManualSignal()
{
	return !_noSignalDetected;
}
