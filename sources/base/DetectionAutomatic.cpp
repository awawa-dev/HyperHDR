/* DetectionAutomatic.cpp
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
	#include <cmath>
#endif

#include <base/Grabber.h>
#include <base/GrabberWrapper.h>
#include <base/HyperHdrManager.h>

DetectionAutomatic::DetectionAutomatic() :
	_log(Logger::getInstance("SIGNAL_AUTO")),
	_saveResources(false),
	_errorTolerance(9),
	_modelTolerance(90),
	_sleepTime(1000),
	_wakeTime(0),
	_noSignal(false),
	_offSignalTime(0),
	_onSignalTime(0),
	_backupDecimation(0)
{
};

QJsonDocument DetectionAutomatic::startCalibration()
{
	if (!calibrationData.isRunning)
	{
		calibrationData.reset();

		calibrationData.backupHDR = getHdrToneMappingEnabled();

		if (_backupDecimation > 0)
			calibrationData.decimationFPS = _backupDecimation;
		else
			calibrationData.decimationFPS = std::max(getFpsSoftwareDecimation(), 1);

		setHdrToneMappingEnabled(0);
		setFpsSoftwareDecimation(1);
		calibrationData.tolerance = _errorTolerance;
		calibrationData.model = _modelTolerance;
		calibrationData.isRunning = true;

		Info(_log, "%s", QSTRING_CSTR(calibrationData.status));
	}

	QJsonObject val;
	val["running"] = calibrationData.isRunning;
	val["status"] = calibrationData.status;
	QJsonDocument doc(val);
	return doc;
}

QJsonDocument DetectionAutomatic::stopCalibration()
{
	if (calibrationData.isRunning)
	{
		calibrationData.isRunning = false;
		calibrationData.status = "Calibration was interrupted";
		setHdrToneMappingEnabled(calibrationData.backupHDR);
		setFpsSoftwareDecimation(calibrationData.decimationFPS);
		Warning(_log, "%s", QSTRING_CSTR(calibrationData.status));
	}

	QJsonObject val;
	val["running"] = calibrationData.isRunning;
	val["status"] = calibrationData.status;
	QJsonDocument doc(val);
	return doc;
}

QJsonDocument DetectionAutomatic::getCalibrationInfo()
{
	QJsonObject val;
	val["isSuccess"] = calibrationData.isSuccess;
	val["running"] = calibrationData.isRunning;
	val["status"] = calibrationData.status;

	val["sdrStat"] = calibrationData.sdrStat;
	val["hdrStat"] = calibrationData.hdrStat;
	val["sdrPoint"] = (int)calibrationData.sdrPoint.size();
	val["hdrPoint"] = (int)calibrationData.hdrPoint.size();
	val["model"] = calibrationData.model;
	val["quality"] = calibrationData.quality;

	QJsonDocument doc(val);
	return doc;
}

bool DetectionAutomatic::isCalibrating()
{
	return calibrationData.isRunning;
}

bool DetectionAutomatic::checkSignalDetectionAutomatic(Image<ColorRgb>& image)
{
	bool result = true;

	if (calibrationData.isRunning)
		calibrateFrame(image);
	else
		result = checkSignal(image);

	return result;
}

DetectionAutomatic::calibration::calibration()
{
	reset();
	status = "";
}

void DetectionAutomatic::calibration::reset()
{
	isSuccess = false;
	isRunning = false;

	sdrStat = 0;
	hdrStat = 0;

	backupHDR = 0;
	decimationFPS = 0;
	phaseStartTime = InternalClock::now() + 200;
	phaseEndTime = InternalClock::now() + 5000 + 200;
	endTime = InternalClock::now() + 60 * 1000;
	currentSDRframe = 0;
	currentHDRframe = 0;
	signature = "";
	width = -1;
	height = -1;
	tolerance = 0;
	model = 0;
	quality = 0;
	status = "Waiting for the first SDR frame";

	currentPhase = calibrationPhase::WAITING_FOR_SDR;

	sdrPoint.clear();
	hdrPoint.clear();
};

DetectionAutomatic::calibrationPoint::calibrationPoint(int _x, int _y)
{
	init = false;
	lost = false;
	x = _x;
	y = _y;
	r = 0;
	g = 0;
	b = 0;
}

DetectionAutomatic::calibrationPoint::calibrationPoint()
{
	init = false;
	lost = false;
	x = 0;
	y = 0;
	r = 0;
	g = 0;
	b = 0;
}

void DetectionAutomatic::calibration::buildPoints(QString _signature, int _width, int _height)
{
	signature = _signature;
	width = _width;
	height = _height;
	sdrPoint.clear();
	hdrPoint.clear();

	for (int py = 0; py < 10; py++)
		for (int px = 0; px < 16; px++)
		{
			int x = std::min((int)std::max((width * (px / 2)) / 8 + (width * (1 + (px % 2))) / (8 * 3), 1), width - 1);
			int y = std::min((int)std::max((height * py) / 10 + height * 0.05, 1.0), height - 1);

			calibrationPoint cp(x, y);

			sdrPoint.push_back(cp);
			hdrPoint.push_back(cp);
		}

	int sy = std::min((int)std::max((height * 4.3) / 10 + height * 0.05, 1.0), height - 1) + 1;
	int ey = std::min((int)std::max((height * 4.8) / 10 + height * 0.05, 1.0), height - 1) - 1;
	int step = std::max((ey - sy) / 8, 1);
	int delta = 0;
	for (int py = sy + step; py < ey; py += step, delta++)
	{
		for (int px = 10; px < 28; px++)
		{
			int x = std::min((int)std::max((width * (px / 5)) / 8 + (width * (1 + (px % 5))) / (8 * 6), 1), width - 1);
			if (delta % 2)
				x = std::min((int)std::max((width * (px / 5)) / 8 + (width * (1 + 2 * (px % 5))) / (8 * 12), 1), width - 1);

			calibrationPoint cp(x, py);

			sdrPoint.push_back(cp);
			hdrPoint.push_back(cp);
		}
	}
};

void DetectionAutomatic::calibrateFrame(Image<ColorRgb>& image)
{
	qint64 time = InternalClock::now();

	if (time > calibrationData.endTime || time > calibrationData.phaseEndTime)
	{
		calibrationData.status = "Timeout out waiting for frames";
		calibrationData.isRunning = false;
		setHdrToneMappingEnabled(calibrationData.backupHDR);
		setFpsSoftwareDecimation(calibrationData.decimationFPS);
		Error(_log, "The calibration is finished. %s", QSTRING_CSTR(calibrationData.status));
		return;
	}

	if (time < calibrationData.phaseStartTime)
	{
		calibrationData.status = QString("Preparing for capturing %1 frame").arg(
			(calibrationData.currentPhase == calibrationPhase::WAITING_FOR_SDR) ? "SDR" : "HDR");
		return;
	}

	if (calibrationData.width > 0 && calibrationData.height > 0 &&
		(calibrationData.width != (int)image.width() || calibrationData.height != (int)image.height()))
	{
		calibrationData.status = "Stream width or height has changed during calibration";
		calibrationData.isRunning = false;
		setHdrToneMappingEnabled(calibrationData.backupHDR);
		setFpsSoftwareDecimation(calibrationData.decimationFPS);
		Error(_log, "The calibration is finished. %s", QSTRING_CSTR(calibrationData.status));
		return;
	}

	if (calibrationData.currentPhase == calibrationPhase::WAITING_FOR_SDR && getHdrToneMappingEnabled() == 0)
	{
		calibrationData.buildPoints(getSignature(), image.width(), image.height());
		calibrationData.phaseEndTime = calibrationData.endTime;
		calibrationData.currentPhase = calibrationPhase::CALIBRATING_SDR;
		calibrationData.status = "Capturing SDR frames";
		Debug(_log, "%s", QSTRING_CSTR(calibrationData.status));
	}
	else if (calibrationData.currentPhase == calibrationPhase::WAITING_FOR_HDR && getHdrToneMappingEnabled() == 1)
	{
		calibrationData.phaseEndTime = calibrationData.endTime;
		calibrationData.currentPhase = calibrationPhase::CALIBRATING_HDR;
		calibrationData.status = "Capturing HDR frames";
		Debug(_log, "%s", QSTRING_CSTR(calibrationData.status));
	}
	else if (calibrationData.currentPhase == calibrationPhase::CALIBRATING_SDR ||
		calibrationData.currentPhase == calibrationPhase::CALIBRATING_HDR)
	{
		std::vector<calibrationPoint>& points = (calibrationData.currentPhase == calibrationPhase::CALIBRATING_SDR) ?
			calibrationData.sdrPoint : calibrationData.hdrPoint;
		for (calibrationPoint& p : points)
		{
			if (!p.init)
			{
				auto col = image(p.x, p.y);
				p.r = col.red;
				p.g = col.green;
				p.b = col.blue;
				p.init = true;
			}
			else if (!p.lost)
			{
				ColorRgb& col = image(p.x, p.y);
				int distance = std::abs(p.r - col.red) + std::abs(p.g - col.green) + std::abs(p.b - col.blue);
				if (distance > calibrationData.tolerance)
					p.lost = true;
				else
				{
					image.fastBox(p.x - 1, p.y - 1, p.x + 1, p.y + 1,
						col.red ^ 255, col.green ^ 255, col.blue ^ 255);
				}
			}
		}

		if (calibrationData.currentPhase == calibrationPhase::CALIBRATING_SDR)
		{
			calibrationData.currentSDRframe++;
			calibrationData.status = QString("Processing SDR frames: %1 / 100").arg(calibrationData.currentSDRframe);
			if (calibrationData.currentSDRframe == 100)
			{
				setHdrToneMappingEnabled(1);
				calibrationData.phaseStartTime = InternalClock::now() + 500;
				calibrationData.phaseEndTime = InternalClock::now() + 3000 + 500;
				calibrationData.currentPhase = calibrationPhase::WAITING_FOR_HDR;
				calibrationData.status = "Waiting for first HDR frame";
				Debug(_log, "%s", QSTRING_CSTR(calibrationData.status));
			}
		}
		else
		{
			calibrationData.currentHDRframe++;
			calibrationData.status = QString("Processing HDR frames: %1 / 100").arg(calibrationData.currentHDRframe);
			if (calibrationData.currentHDRframe == 100)
			{
				QString res;
				calibrationData.sdrStat = 0;
				calibrationData.hdrStat = 0;

				for (calibrationPoint& p : calibrationData.sdrPoint)
					if (!p.lost)
						calibrationData.sdrStat++;
				for (calibrationPoint& p : calibrationData.hdrPoint)
					if (!p.lost)
						calibrationData.hdrStat++;

				int aspect = 100 - ((calibrationData.tolerance / 10) * 10);
				double sdrQuality = calibrationData.sdrStat * 100.0 / std::max((double)calibrationData.sdrPoint.size(), 1.0);
				double hdrQuality = calibrationData.hdrStat * 100.0 / std::max((double)calibrationData.hdrPoint.size(), 1.0);
				calibrationData.quality = (int)((std::min(std::min(sdrQuality, hdrQuality), 100.0) * aspect) / 100.0);

				if (calibrationData.quality < _modelTolerance)
				{
					calibrationData.isSuccess = false;
					res = QString("failed. You requested quality at %1% but it's only %2%. Try to increase the error tolerance and/or decrease the cognition model requirement. Then make sure that the grabber is displaying the 'no signal' board").arg(calibrationData.model).arg(calibrationData.quality);
				}
				else
				{
					calibrationData.isSuccess = true;
					res = QString("succeed. The cognition model quality is %1%").arg(calibrationData.quality);
				}

				calibrationData.status = QString("The calibration %5. Got %1 SDR test points (from %3) and %2 HDR test points (from %4).").
					arg(calibrationData.sdrStat).arg(calibrationData.hdrStat).arg(calibrationData.sdrPoint.size()).arg(calibrationData.hdrPoint.size()).arg(res);
				Debug(_log, "%s", QSTRING_CSTR(calibrationData.status));

				setHdrToneMappingEnabled(calibrationData.backupHDR);
				setFpsSoftwareDecimation(calibrationData.decimationFPS);

				saveResult();

				calibrationData.isRunning = false;
			}
		}
	}
}


void DetectionAutomatic::setAutoSignalParams(bool saveResources, int errorTolerance, int modelTolerance, int sleepTime, int wakeTime)
{
	_saveResources = saveResources;
	_errorTolerance = errorTolerance;
	_modelTolerance = modelTolerance;
	_sleepTime = sleepTime;
	_wakeTime = wakeTime;
	Debug(_log, "Automatic signal detection -> errorTolerance: %i, modelTolerance: %i, sleepTime: %i, wakeTime: %i",
		_errorTolerance, _modelTolerance, _sleepTime, _wakeTime);
}

void DetectionAutomatic::saveResult()
{
	if (calibrationData.isSuccess)
	{
		QJsonObject obj;
		QJsonArray sdr, hdr;

		for (calibrationPoint& p : calibrationData.sdrPoint)
			if (!p.lost)
			{
				QJsonObject point;
				QJsonArray color;

				color.append(p.r);
				color.append(p.g);
				color.append(p.b);
				point["x"] = p.x;
				point["y"] = p.y;
				point["color"] = color;

				sdr.append(point);
			}
		for (calibrationPoint& p : calibrationData.hdrPoint)
			if (!p.lost)
			{
				QJsonObject point;
				QJsonArray color;

				color.append(p.r);
				color.append(p.g);
				color.append(p.b);
				point["x"] = p.x;
				point["y"] = p.y;
				point["color"] = color;

				hdr.append(point);
			}

		obj["signature"] = calibrationData.signature;
		obj["quality"] = calibrationData.quality;
		obj["width"] = calibrationData.width;
		obj["height"] = calibrationData.height;
		obj["calibration_sdr"] = sdr;
		obj["calibration_hdr"] = hdr;

		QString saveData = QString(QJsonDocument(obj).toJson(QJsonDocument::Compact));
		emit SignalSaveCalibration(saveData);
	}
}

void DetectionAutomatic::setAutomaticCalibrationData(QString signature, int quality, int width, int height,
	std::vector<DetectionAutomatic::calibrationPoint> sdrVec, std::vector<DetectionAutomatic::calibrationPoint> hdrVec)
{
	resetStats();
	checkData.reset();
	checkData.signature = signature;
	checkData.quality = quality;
	checkData.width = width;
	checkData.height = height;
	checkData.sdrPoint = sdrVec;
	checkData.hdrPoint = hdrVec;
}

void DetectionAutomatic::resetStats()
{
	_noSignal = false;
	_offSignalTime = 0;
	_onSignalTime = 0;
}

bool DetectionAutomatic::checkSignal(Image<ColorRgb>& image)
{
	int hdrMode = getHdrToneMappingEnabled();

	if (checkData.width <= 0 || checkData.height <= 0 || checkData.quality <= 0 || checkData.sdrPoint.size() == 0 || checkData.hdrPoint.size() == 0)
	{
		QString oldStatus = checkData.status;
		checkData.status = "Don't have a calibration data. Please run the calibration procedure in the panel grabber tab.";
		if (oldStatus != checkData.status)
		{
			Error(_log, "%s", QSTRING_CSTR(checkData.status));
			resetStats();
		}
		return true;
	}

	if (checkData.width != (int)image.width() || checkData.height != (int)image.height())
	{
		QString oldStatus = checkData.status;
		checkData.status = "Calibration data is not meant for the current video stream (different dimensions). Please run the calibration procedure in the panel grabber tab.";
		if (oldStatus != checkData.status)
		{
			Error(_log, "%s", QSTRING_CSTR(checkData.status));
			resetStats();
		}
		return true;
	}

	if (hdrMode > 1)
	{
		QString oldStatus = checkData.status;
		checkData.status = "Automatic detection doesn't work for the HDR border mode";
		if (oldStatus != checkData.status)
		{
			Error(_log, "%s", QSTRING_CSTR(checkData.status));
			resetStats();
		}
		return true;
	}

	if (checkData.signature != getSignature())
	{
		QString oldStatus = checkData.status;
		checkData.status = "Calibration data signature is different from the current stream signature. Just a warning. Please run the calibration procedure in the panel grabber tab.";
		if (oldStatus != checkData.status)
			Warning(_log, "%s (have: '%s' <> current: '%s')", QSTRING_CSTR(checkData.status), QSTRING_CSTR(checkData.signature), QSTRING_CSTR(getSignature()));
	}

	std::vector<DetectionAutomatic::calibrationPoint>& data = (hdrMode == 0) ? checkData.sdrPoint : checkData.hdrPoint;
	int _on = 0, _off = 0;

	for (const auto& v : data)
	{
		if (!image.checkSignal(v.x, v.y, v.r, v.g, v.b, _errorTolerance))
			_off++;
		else
			_on++;
	}

	int finalQuality = (_on * checkData.quality) / (_on + _off);
	bool hasSignal = (finalQuality <= _modelTolerance);

	if (!hasSignal && _noSignal)
	{
		_onSignalTime = 0;
		return false;
	}
	if (hasSignal && !_noSignal)
	{
		_offSignalTime = 0;
		return true;
	}

	qint64 time = InternalClock::now();

	if (!hasSignal && !_noSignal)
	{
		if (_offSignalTime == 0)
		{
			Info(_log, "No signal detected. The cognition model's probability: %i%%", finalQuality);
			_offSignalTime = time;
		}

		if (time - _offSignalTime >= _sleepTime)
		{
			if (_saveResources)
			{
				_backupDecimation = getFpsSoftwareDecimation();

				int currentFPS = getActualFps();

				int relax = std::max(currentFPS / 3, 1);

				setFpsSoftwareDecimation(relax);
			}

			Info(_log, "THE SIGNAL IS LOST");
			_noSignal = true;
			_offSignalTime = 0;
			return false;
		}

		return true;
	}
	else
		_offSignalTime = 0;

	if (hasSignal && _noSignal)
	{
		if (_onSignalTime == 0)
		{
			Info(_log, "Signal detected. The cognition model's probability: %i%%", std::max(100 - finalQuality, 0));
			_onSignalTime = time;
		}

		if (time - _onSignalTime >= _wakeTime)
		{
			if (_backupDecimation > 0)
			{
				setFpsSoftwareDecimation(_backupDecimation);
				_backupDecimation = 0;
			}

			Info(_log, "THE SIGNAL IS RESTORED");
			_noSignal = false;
			_onSignalTime = 0;
			return true;
		}

		return false;
	}
	else
		_onSignalTime = 0;

	return true;
}

bool DetectionAutomatic::getDetectionAutoSignal()
{
	return _noSignal;
}
