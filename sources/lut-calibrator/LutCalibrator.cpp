/* LutCalibrator.cpp
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

#ifndef PCH_ENABLED
	#include <QJsonArray>
	#include <QJsonObject>
	#include <QFile>
	#include <QDateTime>
	#include <QThread>

	#include <cmath>
	#include <cfloat>
	#include <climits>

	#include <utils/Logger.h>
#endif

#define STRING_CSTR(x) (x.operator std::string()).c_str()

#include <QCoreApplication>

#include <lut-calibrator/LutCalibrator.h>
#include <utils/GlobalSignals.h>
#include <base/GrabberWrapper.h>
#include <api/HyperAPI.h>
#include <base/HyperHdrManager.h>
#include <led-strip/ColorSpaceCalibration.h>

ColorRgb LutCalibrator::primeColors[] = {
				ColorRgb(255, 0, 0), ColorRgb(0, 255, 0), ColorRgb(0, 0, 255), ColorRgb(255, 255, 0),
				ColorRgb(255, 0, 255), ColorRgb(0, 255, 255),ColorRgb(255, 128, 0), ColorRgb(255, 0, 128), ColorRgb(0, 128, 255),
				ColorRgb(128, 64, 0), ColorRgb(128, 0, 64),
				ColorRgb(128, 0, 0),ColorRgb(0, 128, 0),ColorRgb(0, 0, 128),
				ColorRgb(16, 16, 16), ColorRgb(32, 32, 32), ColorRgb(48, 48, 48), ColorRgb(64, 64, 64), ColorRgb(96, 96, 96), ColorRgb(120, 120, 120), ColorRgb(144, 144, 144), ColorRgb(172, 172, 172), ColorRgb(196, 196, 196), ColorRgb(220, 220, 220),
				ColorRgb(255, 255, 255),
				ColorRgb(0, 0, 0)
};


#define LUT_FILE_SIZE 50331648
#define LUT_INDEX(y,u,v) ((y + (u<<8) + (v<<16))*3)
#define REC(x) (x == 2) ? "REC.601" : (x == 1) ? "REC.709" : "FCC"
LutCalibrator* LutCalibrator::instance = nullptr;


LutCalibrator::LutCalibrator()
{
	_log = Logger::getInstance("CALIBRATOR");
	_mjpegCalibration = false;
	_finish = false;
	_limitedRange = false;
	_saturation = 1;
	_luminance = 1;
	_gammaR = 1;
	_gammaG = 1;
	_gammaB = 1;
	_checksum = -1;
	_currentCoef = 0;
	std::fill_n(_coefsResult, sizeof(_coefsResult) / sizeof(double), 0.0);
	_warningCRC = _warningMismatch = -1;
	_startColor = ColorRgb(0, 0, 0);
	_endColor = ColorRgb(0, 0, 0);
	_minColor = ColorRgb(255, 255, 255);
	for (capColors selector = capColors::Red; selector != capColors::None; selector = capColors(((int)selector) + 1))
		_colorBalance[(int)selector].reset();
	_maxColor = ColorRgb(0, 0, 0);
	_timeStamp = 0;	
}

LutCalibrator::~LutCalibrator()
{	
}

void LutCalibrator::incomingCommand(QString rootpath, GrabberWrapper* grabberWrapper, hyperhdr::Components defaultComp, int checksum, ColorRgb startColor, ColorRgb endColor, bool limitedRange, double saturation, double luminance, double gammaR, double gammaG, double gammaB, int coef)
{
	_rootPath = rootpath;

	if (checksum == 0)
	{
		if (grabberWrapper != nullptr && !_mjpegCalibration)
		{
			if (grabberWrapper->getHdrToneMappingEnabled() != 0)
			{
				QJsonObject report;
				stopHandler();
				Error(_log, "Please disable LUT tone mapping and run the test again");
				report["status"] = 1;
				report["error"] = "Please disable LUT tone mapping and run the test again";
				SignalLutCalibrationUpdated(report);
				return;
			}

			QString vidMode;

			SAFE_CALL_0_RET(grabberWrapper, getVideoCurrentModeResolution, QString, vidMode);

			_mjpegCalibration = (vidMode.indexOf("mjpeg", 0, Qt::CaseInsensitive) >= 0);

			if (_mjpegCalibration)
			{
				Debug(_log, "Enabling pseudo-HDR mode for calibration to bypass TurboJPEG MJPEG to RGB processing");

				emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);

				int hdrEnabled = 0;
				SAFE_CALL_0_RET(grabberWrapper, getHdrToneMappingEnabled, int, hdrEnabled);
				Debug(_log, "HDR is %s", (hdrEnabled) ? "enabled" : "disabled");

				if (!hdrEnabled)
				{
					QJsonObject report;
					stopHandler();
					Error(_log, "Unexpected HDR state. Aborting");
					report["status"] = 1;
					report["error"] = "Unexpected HDR state. Aborting";
					SignalLutCalibrationUpdated(report);
					return;
				}
			}
		}

		_finish = false;
		_limitedRange = limitedRange;
		_saturation = saturation;
		_luminance = luminance;
		_gammaR = gammaR;
		_gammaG = gammaG;
		_gammaB = gammaB;
		_checksum = -1;
		_currentCoef = coef % (sizeof(_coefsResult) / sizeof(double));
		_warningCRC = _warningMismatch = -1;
		_minColor = ColorRgb(255, 255, 255);
		_maxColor = ColorRgb(0, 0, 0);
		for (capColors selector = capColors::Red; selector != capColors::None; selector = capColors(((int)selector) + 1))
			_colorBalance[(int)selector].reset();

		
		_lut.resize(LUT_FILE_SIZE * 2);

		if (_lut.data() != nullptr)
		{
			memset(_lut.data(), 0, LUT_FILE_SIZE * 2);

			finalize(true);
			memset(_lut.data(), 0, LUT_FILE_SIZE * 2);
			
			if (grabberWrapper != nullptr)
			{
				_log->disable();

				BLOCK_CALL_0(grabberWrapper, stop);
				BLOCK_CALL_0(grabberWrapper, start);

				QThread::msleep(2000);

				_log->enable();
			}
			else
			{
				Debug(_log, "Reloading LUT 1/2");
				emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);
				QThread::msleep(2000);
				Debug(_log, "Reloading LUT 2/2");
				emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, false);
				QThread::msleep(2000);
				Debug(_log, "Finished reloading LUT");
			}

			if (defaultComp == hyperhdr::COMP_VIDEOGRABBER)
			{
				Debug(_log, "Using video grabber as a source");
				connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &LutCalibrator::setVideoImage, Qt::ConnectionType::UniqueConnection);
			}
			else if (defaultComp == hyperhdr::COMP_SYSTEMGRABBER)
			{
				Debug(_log, "Using system grabber as a source");
				connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &LutCalibrator::setSystemImage, Qt::ConnectionType::UniqueConnection);
			}
			else
			{
				Debug(_log, "Using flatbuffers/protobuffers as a source");
				connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &LutCalibrator::signalSetGlobalImageHandler, Qt::ConnectionType::UniqueConnection);
			}
		}
		else
		{
			QJsonObject report;
			stopHandler();
			Error(_log, "Could not allocated memory (~100MB) for internal temporary buffer. Stopped.");
			report["status"] = 1;
			report["error"] = "Could not allocated memory (~100MB) for internal temporary buffer. Stopped.";
			SignalLutCalibrationUpdated(report);
			return;
		}
	}
	_checksum = checksum;
	_startColor = startColor;
	_endColor = endColor;
	_timeStamp = InternalClock::now();

	if (_checksum % 19 == 1)
		Debug(_log, "Requested section: %i, %s, %s, YUV: %s, Coef: %s, Saturation: %f, Luminance: %f, Gammas: (%f, %f, %f)",
						_checksum, STRING_CSTR(_startColor), STRING_CSTR(_endColor), (_limitedRange) ? "LIMITED" : "FULL",
						REC(_currentCoef), _saturation, _luminance, _gammaR, _gammaG, _gammaB);
}

void LutCalibrator::stopHandler()
{
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &LutCalibrator::setVideoImage);
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &LutCalibrator::signalSetGlobalImageHandler);
	_mjpegCalibration = false;
	_finish = false;
	_checksum = -1;
	_warningCRC = _warningMismatch = -1;
	std::fill_n(_coefsResult, sizeof(_coefsResult) / sizeof(double), 0.0);

	_lut.releaseMemory();
}

void LutCalibrator::setVideoImage(const QString& name, const Image<ColorRgb>& image)
{
	handleImage(image);
}

void LutCalibrator::setSystemImage(const QString& name, const Image<ColorRgb>& image)
{
	handleImage(image);
}

void LutCalibrator::signalSetGlobalImageHandler(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin)
{
	handleImage(image);
}

void LutCalibrator::handleImage(const Image<ColorRgb>& image)
{
	int validate = 0;
	int diffColor = 0;
	QJsonObject report;
	QJsonArray colors;
	ColorRgb white{ 128,128,128 }, black{ 16,16,16 };
	double scaleX = image.width() / 128.0;
	double scaleY = image.height() / 72.0;

	if (_checksum < 0)
		return;

	if (image.width() < 3 * 128 || image.height() < 3 * 72)
	{
		stopHandler();
		Error(_log, "Too low resolution: 384x216 is the minimum. Received video frame: %ix%i. Stopped.", image.width(), image.height());
		report["status"] = 1;
		report["error"] = "Too low resolution: 384x216 is the minimum. Received video frame: %ix%i. Stopped.";
		SignalLutCalibrationUpdated(report);
		return;
	}

	if (image.width() * 1080 != image.height() * 1920)
	{
		stopHandler();
		Error(_log, "Invalid resolution width/height ratio. Expected aspect: 1920/1080 (or the same 1280/720 etc). Stopped.");
		report["status"] = 1;
		report["error"] = "Invalid resolution width/height ratio. Expected aspect: 1920/1080 (or the same 1280/720 etc). Stopped.";
		SignalLutCalibrationUpdated(report);
		return;
	}

	for (int py = 0; py < 72;)
	{
		for (int px = (py < 71 && py > 0) ? _checksum % 2 : 0; px < 128; px++)
		{
			ColorRgb	color;
			int sX = (qRound(px * scaleX) + qRound((px + 1) * scaleX)) / 2;
			int sY = (qRound(py * scaleY) + qRound((py + 1) * scaleY)) / 2;
			int cR = 0, cG = 0, cB = 0;

			for (int i = -1; i <= 1; i++)
				for (int j = -1; j <= 1; j++)
				{
					ColorRgb cur = image(sX + i, sY + j);
					cR += cur.red;
					cG += cur.green;
					cB += cur.blue;
				}

			color = ColorRgb((uint8_t)qMin(qRound(cR / 9.0), 255), (uint8_t)qMin(qRound(cG / 9.0), 255), (uint8_t)qMin(qRound(cB / 9.0), 255));


			if (py < 71 && py > 0)
			{
				if (!_finish)
				{
					storeColor(_startColor, color);
					_finish |= increaseColor(_startColor);
				}
				else
					increaseColor(_startColor);
			}
			else
			{
				if (px == 0)
				{
					white = color;
					validate = 0;
					diffColor = qMax(white.red, qMax(white.green, white.blue));
					diffColor = qMax((diffColor * 5) / 100, 10);
				}
				else if (px == 1)
				{
					black = color;
				}
				else if (px >= 8 && px < 24)
				{
					bool isWhite = ((diffColor + color.red) >= white.red &&
						(diffColor + color.green) >= white.green &&
						(diffColor + color.blue) >= white.blue);

					bool isBlack = (color.red <= (black.red + diffColor) &&
						color.green <= (black.green + diffColor) &&
						color.blue <= (black.blue + diffColor));

					if ((isWhite && isBlack) || (!isWhite && !isBlack))
					{
						if (_warningCRC != _checksum && (InternalClock::now() - _timeStamp > 1000))
						{
							_warningCRC = _checksum;
							Warning(_log, "Invalid CRC at: %i. CurrentColor: %s, Black: %s, White: %s, StartColor: %s, EndColor: %s.", int(px - 8),
								STRING_CSTR(color), STRING_CSTR(black),
								STRING_CSTR(white), STRING_CSTR(_startColor),
								STRING_CSTR(_endColor));
						}
						return;
					}

					auto sh = (isWhite) ? 1 << (15 - (px - 8)) : 0;

					validate |= sh;
				}
				else if (px == 24)
				{
					if (validate != _checksum)
					{
						if (_warningMismatch != _checksum && (InternalClock::now() - _timeStamp > 1000))
						{
							_warningMismatch = _checksum;
							Warning(_log, "CRC does not match: %i but expected %i, StartColor: %s , EndColor: %s", validate, _checksum,
								STRING_CSTR(_startColor), STRING_CSTR(_endColor));
						}
						return;
					}
				}
			}
		}

		switch (py)
		{
			case(0): py = 71; break;
			case(70): py = 72; break;
			case(71): py = 1; break;
			default: py++; break;
		}
	}

	if (_startColor != _endColor)
	{
		stopHandler();
		Error(_log, "Unexpected color %s != %s", STRING_CSTR(_startColor), STRING_CSTR(_endColor));
		report["status"] = 1;
		SignalLutCalibrationUpdated(report);
		return;
	}


	report["status"] = 0;
	report["validate"] = validate;

	if (_checksum % 19 == 1)
	{
		Info(_log, "The video frame has been analyzed. Progress: %i/21 section", _checksum);
	}

	_checksum = -1;

	if (_finish)
	{
		if (!correctionEnd())
			return;

		if (!finalize())
			report["status"] = 1;
	}

	SignalLutCalibrationUpdated(report);
}

void LutCalibrator::storeColor(const ColorRgb& inputColor, const ColorRgb& color)
{

	for (capColors selector = capColors::Red; selector != capColors::None; selector = capColors(((int)selector) + 1))
		if (inputColor == primeColors[(int)selector])
		{
			_colorBalance[(int)selector].AddColor(color);
			break;
		}

	if (inputColor.red == inputColor.blue && inputColor.green == inputColor.blue)
	{
		if (color.red > _maxColor.red)		_maxColor.red = color.red;
		if (color.green > _maxColor.green)  _maxColor.green = color.green;
		if (color.blue > _maxColor.blue)	_maxColor.blue = color.blue;
		if (color.red < _minColor.red)		_minColor.red = color.red;
		if (color.green < _minColor.green)  _minColor.green = color.green;
		if (color.blue < _minColor.blue)	_minColor.blue = color.blue;
	}
}

bool LutCalibrator::increaseColor(ColorRgb& color)
{
	if (color == primeColors[capColors::None])
		color = primeColors[0];
	else
	{
		for (capColors selector = capColors::Red; selector != capColors::None; selector = capColors(((int)selector) + 1))
			if (color == primeColors[(int)selector])
			{
				color = primeColors[(int)capColors(((int)selector) + 1)];
				break;
			}
	}
	return (_checksum > 20) ? true : false;
}


double LutCalibrator::eotf(double scale, double  x) noexcept
{
	// https://github.com/sekrit-twc/zimg/blob/master/src/zimg/colorspace/gamma.cpp

	constexpr double ST2084_M1 = 0.1593017578125;
	constexpr double ST2084_M2 = 78.84375;
	constexpr double ST2084_C1 = 0.8359375;
	constexpr double ST2084_C2 = 18.8515625;
	constexpr double ST2084_C3 = 18.6875;

	if (x > 0.0) {
		double xpow = std::pow(x, 1.0 / ST2084_M2);
		double num = std::max(xpow - ST2084_C1, 0.0);
		double den = std::max(ST2084_C2 - ST2084_C3 * xpow, DBL_MIN);
		x = std::pow(num / den, 1.0 / ST2084_M1);
	}
	else {
		x = 0.0;
	}

	return scale * x;
}

double LutCalibrator::inverse_eotf(double x) noexcept
{
	// https://github.com/sekrit-twc/zimg/blob/master/src/zimg/colorspace/gamma.cpp

	constexpr double ST2084_M1 = 0.1593017578125;
	constexpr double ST2084_M2 = 78.84375;
	constexpr double ST2084_C1 = 0.8359375;
	constexpr double ST2084_C2 = 18.8515625;
	constexpr double ST2084_C3 = 18.6875;

	if (x > 0.0f) {
		double xpow = std::pow(x, ST2084_M1);
		double num = (ST2084_C1 - 1.0) + (ST2084_C2 - ST2084_C3) * xpow;
		double den = 1.0 + ST2084_C3 * xpow;
		x = std::pow(1.0 + num / den, ST2084_M2);
	}
	else {
		x = 0.0f;
	}

	return x;
}

double LutCalibrator::ootf(double v) noexcept
{
	// https://github.com/sekrit-twc/zimg/blob/master/src/zimg/colorspace/gamma.cpp	

	constexpr double SRGB_ALPHA = 1.055010718947587;
	constexpr double SRGB_BETA = 0.003041282560128;

	v = std::max(v, 0.0);

	if (v < SRGB_BETA)
		return v * 12.92;
	else
		return SRGB_ALPHA * std::pow(v, 1.0 / 2.4) - (SRGB_ALPHA - 1.0);
}

double LutCalibrator::inverse_gamma(double x) noexcept
{
	// https://github.com/sekrit-twc/zimg/blob/master/src/zimg/colorspace/gamma.cpp

	x = std::max(x, 0.0);

	x = std::pow(x, 1.0 / 2.4);

	return x;
}


void LutCalibrator::fromBT2020toXYZ(double r, double g, double b, double& x, double& y, double& z)
{
	x = 0.636958 * r + 0.144617 * g + 0.168881 * b;
	y = 0.262700 * r + 0.677998 * g + 0.059302 * b;
	z = 0.000000 * r + 0.028073 * g + 1.060985 * b;
}

void LutCalibrator::fromXYZtoBT709(double x, double y, double z, double& r, double& g, double& b)
{
	r = 3.240970 * x - 1.537383 * y - 0.498611 * z;
	g = -0.969244 * x + 1.875968 * y + 0.041555 * z;
	b = 0.055630 * x - 0.203977 * y + 1.056972 * z;
}

void LutCalibrator::fromBT2020toBT709(double x, double y, double z, double& r, double& g, double& b)
{
	r = 1.6605 * x - 0.5876 * y - 0.0728 * z;
	g = -0.1246 * x + 1.1329 * y - 0.0083 * z;
	b = -0.0182 * x - 0.1006 * y + 1.1187 * z;
}

void LutCalibrator::toneMapping(double xHdr, double yHdr, double zHdr, double& xSdr, double& ySdr, double& zSdr)
{
	double mian = xHdr + yHdr + zHdr;

	if (mian < 0.000001)
	{
		xSdr = 0;
		ySdr = 0;
		zSdr = 0;
		return;
	}

	double k1 = 0.83802, k2 = 15.09968, k3 = 0.74204, k4 = 78.99439;
	double check = 58.5 / k1;
	double x = xHdr / mian;
	double y = yHdr / mian;

	if (yHdr < check)
		ySdr = qMax(k1 * yHdr, 0.0);
	else
		ySdr = qMax(k2 * std::log(yHdr / check - k3) + k4, 0.0);

	xSdr = (x / y) * ySdr;
	zSdr = ((1 - x - y) / y) * ySdr;

}

void LutCalibrator::colorCorrection(double& r, double& g, double& b)
{
	double hue, saturation, luminance;

	ColorSpaceCalibration::rgb2hsl_d(r, g, b, hue, saturation, luminance);

	double s = saturation * _saturation;
	saturation = s;

	double l = luminance * _luminance;
	luminance = l;

	ColorSpaceCalibration::hsl2rgb_d(hue, saturation, luminance, r, g, b);
}

void LutCalibrator::balanceGray(int r, int g, int b, double& _r, double& _g, double& _b)
{
	if ((_r == 0 && _g == 0 && _b == 0) ||
		(_r == 1 && _g == 1 && _b == 1))
		return;

	int _R = qRound(_r * 255);
	int _G = qRound(_g * 255);
	int _B = qRound(_b * 255);

	int max = qMax(_R, qMax(_G, _B));
	int min = qMin(_R, qMin(_G, _B));


	if (max - min < 30)
	{
		for (capColors selector = capColors::LowestGray; selector != capColors::None; selector = capColors(((int)selector) + 1))
		{
			double whiteR = r * _colorBalance[selector].scaledRed;
			double whiteG = g * _colorBalance[selector].scaledGreen;
			double whiteB = b * _colorBalance[selector].scaledBlue;
			double error = 1;

			if (qAbs(whiteR - whiteG) <= error && qAbs(whiteG - whiteB) <= error && qAbs(whiteB - whiteR) <= error)
			{
				_r = _g = _b = (_r + _g + _b) / 3.0;
				return;
			}
		}

		for (capColors selector = capColors::LowestGray; selector != capColors::None; selector = capColors(((int)selector) + 1))
		{
			double whiteR = r * _colorBalance[selector].scaledRed;
			double whiteG = g * _colorBalance[selector].scaledGreen;
			double whiteB = b * _colorBalance[selector].scaledBlue;
			double error = 2;

			if (qAbs(whiteR - whiteG) <= error && qAbs(whiteG - whiteB) <= error && qAbs(whiteB - whiteR) <= error)
			{
				double average = (_r + _g + _b) / 3.0;
				_r = (_r + average) / 2;
				_g = (_g + average) / 2;
				_b = (_b + average) / 2;
				return;
			}
		}
	}

	if (max <= 4 && max >= 1)
	{
		_r = qMax(_r, 1.0 / 255.0);
		_g = qMax(_g, 1.0 / 255.0);
		_b = qMax(_b, 1.0 / 255.0);
	}
}


QString LutCalibrator::colorToQStr(capColors index)
{
	int ind = (int)index;

	double floor = qMax(_minColor.red, qMax(_minColor.green, _minColor.blue));
	double ceiling = qMax(_maxColor.red, qMax(_maxColor.green, _maxColor.blue)) * 1.05;
	double scale = (ceiling - floor);
	ColorStat normalized = _colorBalance[ind], fNormalized = _colorBalance[ind], color = _colorBalance[ind];
	normalized /= scale;
	fNormalized /= 255.0;
	color.red = qRound(color.red);
	color.green = qRound(color.green);
	color.blue = qRound(color.blue);
	QString retVal = QString("%1 => %2").arg(QString::fromStdString(primeColors[ind])).arg(color.toQString());

	return retVal;
}

double LutCalibrator::getError(ColorRgb first, ColorStat second)
{
	double errorR = 0, errorG = 0, errorB = 0;

	if ((first.red == 255 || first.red == 128) && first.green == 0 && first.blue == 0)
	{
		if (second.red <= 1)
			return std::pow(255, 2) * 3;

		errorR = 0;
		errorG = 100.0 * second.green / second.red;
		errorB = 100.0 * second.blue / second.red;

		if (first.red == 255)
			errorR += (255 - second.red);
	}
	else if (first.red == 0 && (first.green == 255 || first.green == 128)  && first.blue == 0)
	{
		if (second.green <= 1)
			return std::pow(255, 2) * 3;

		errorR = 100.0 * second.red / second.green;
		errorG = 0;
		errorB = 100.0 * second.blue / second.green;

		if (first.green == 255)
			errorG += (255 - second.green);
	}
	else if (first.red == 0 && first.green == 0 && (first.blue == 255 || first.blue == 128))
	{
		if (second.blue <= 1)
			return std::pow(255, 2) * 3;

		errorR = 100.0 * second.red / second.blue;
		errorG = 100.0 * second.green / second.blue;
		errorB = 0;

		if (first.blue == 255)
			errorB += (255 - second.blue);
	}
	else if (first.red == 255 && first.green == 255 && first.blue == 0)
	{
		if (second.red <= 1)
			return std::pow(255, 2) * 3;

		errorR = 0;
		errorG = 0;
		errorB = 2 * 100.0 * second.blue / second.red;
	}
	else if (first.red == 255 && first.green == 0 && first.blue == 255)
	{
		if (second.red <= 1)
			return std::pow(255, 2) * 3;

		errorR = 0;
		errorG = 2 * 100.0 * second.green / second.red;
		errorB = 0;
	}
	else if (first.red == 0 && first.green == 255 && first.blue == 255)
	{
		if (second.blue <= 1)
			return std::pow(255, 2) * 3;

		errorR = 2 * 100.0 * second.red / second.blue;
		errorG = 0;
		errorB = 0;
	}
	else if (first.red == 255 && first.green == 0 && first.blue == 128)
	{
		if (second.red <= 1)
			return std::pow(255, 2) * 3;

		errorR = 2 * 100.0 * qAbs(1 - 2 * second.blue / second.red);
		errorG = 0;
		errorB = 0;
	}
	else if (first.red == 255 && first.green == 128 && first.blue == 0)
	{
		if (second.red <= 1)
			return std::pow(255, 2) * 3;

		errorR = 2 * 100.0 * qAbs(1 - 2 * second.green / second.red);
		errorG = 0;
		errorB = 0;
	}
	else if (first.red == 0 && first.green == 128 && first.blue == 255)
	{
		if (second.blue <= 1)
			return std::pow(255, 2) * 3;

		errorR = 2 * 100.0 * qAbs(1 - 2 * second.green / second.blue);
		errorG = 0;
		errorB = 0;
	}
	else if (first.red == 128 && first.green == 64 && first.blue == 0)
	{
		if (second.red <= 1)
			return std::pow(255, 2) * 3;

		errorR = 0;
		errorG = 2 * 100.0 * qAbs(1 - 2 * second.green / second.red);
		errorB = 0;
	}
	else if (first.red == 128 && first.green == 0 && first.blue == 64)
	{
		if (second.red <= 1)
			return std::pow(255, 2) * 3;

		errorR = 0;
		errorG = 0;
		errorB = 2 * 100.0 * qAbs(1 - 2 * second.blue / second.red);
	}
	else if (first.red == first.green && first.green == first.blue)
	{
		double max = qMax(second.red, qMax(second.green, second.blue)) - qMin(second.red, qMin(second.green, second.blue));

		errorR = max;
		errorG = max;
		errorB = max;

		if (first.red == primeColors[capColors::White].red)
		{
			errorR += qMin(qMax(255 - second.red, 0.0), 10.0);
			errorG += qMin(qMax(255 - second.green, 0.0), 10.0);
			errorB += qMin(qMax(255 - second.blue, 0.0), 10.0);
		}
	}

	return std::pow(errorR, 2) + std::pow(errorG, 2) + std::pow(errorB, 2);
}



QString LutCalibrator::colorToQStr(ColorRgb color)
{
	double mian = qMax(color.red, qMax(color.green, color.blue));

	return QString("(%1, %2, %3), Proportion: (%4, %5, %6)")
		.arg(color.red).arg(color.green).arg(color.blue)
		.arg(color.red / mian, 0, 'f', 3).arg(color.green / mian, 0, 'f', 3).arg(color.blue / mian, 0, 'f', 3);
}

QString LutCalibrator::calColorToQStr(capColors index)
{
	ColorRgb color = primeColors[(int)index], finalColor;
	ColorStat real = _colorBalance[(int)index];

	uint32_t indexRgb = LUT_INDEX(qRound(real.red), qRound(real.green), qRound(real.blue));

	finalColor.red = _lut.data()[indexRgb];
	finalColor.green = _lut.data()[indexRgb + 1];
	finalColor.blue = _lut.data()[indexRgb + 2];

	return QString("%1 => %2").arg(QString::fromStdString(color)).arg(QString::fromStdString(finalColor));
}

void LutCalibrator::displayPreCalibrationInfo()
{
	Debug(_log, "");
	Debug(_log, "--------------------- Captured colors starts ---------------------");
	Debug(_log, "Red: %s", QSTRING_CSTR(colorToQStr(capColors::Red)));
	Debug(_log, "Green: %s", QSTRING_CSTR(colorToQStr(capColors::Green)));
	Debug(_log, "Blue: %s", QSTRING_CSTR(colorToQStr(capColors::Blue)));
	Debug(_log, "Yellow: %s", QSTRING_CSTR(colorToQStr(capColors::Yellow)));
	Debug(_log, "Magenta: %s", QSTRING_CSTR(colorToQStr(capColors::Magenta)));
	Debug(_log, "Cyan: %s", QSTRING_CSTR(colorToQStr(capColors::Cyan)));
	Debug(_log, "Orange: %s", QSTRING_CSTR(colorToQStr(capColors::Orange)));
	Debug(_log, "Pink: %s", QSTRING_CSTR(colorToQStr(capColors::Pink)));
	Debug(_log, "Azure: %s", QSTRING_CSTR(colorToQStr(capColors::Azure)));
	Debug(_log, "Brown: %s", QSTRING_CSTR(colorToQStr(capColors::Brown)));
	Debug(_log, "Purple: %s", QSTRING_CSTR(colorToQStr(capColors::Purple)));
	Debug(_log, "Low red: %s", QSTRING_CSTR(colorToQStr(capColors::LowRed)));
	Debug(_log, "Low green: %s", QSTRING_CSTR(colorToQStr(capColors::LowGreen)));
	Debug(_log, "Low blue: %s", QSTRING_CSTR(colorToQStr(capColors::LowBlue)));
	Debug(_log, "LowestGray: %s", QSTRING_CSTR(colorToQStr(capColors::LowestGray)));
	Debug(_log, "Gray1: %s", QSTRING_CSTR(colorToQStr(capColors::Gray1)));
	Debug(_log, "Gray2: %s", QSTRING_CSTR(colorToQStr(capColors::Gray2)));
	Debug(_log, "Gray3: %s", QSTRING_CSTR(colorToQStr(capColors::Gray3)));
	Debug(_log, "Gray4: %s", QSTRING_CSTR(colorToQStr(capColors::Gray4)));
	Debug(_log, "Gray5: %s", QSTRING_CSTR(colorToQStr(capColors::Gray5)));
	Debug(_log, "Gray6: %s", QSTRING_CSTR(colorToQStr(capColors::Gray6)));
	Debug(_log, "Gray7: %s", QSTRING_CSTR(colorToQStr(capColors::Gray7)));
	Debug(_log, "Gray8: %s", QSTRING_CSTR(colorToQStr(capColors::Gray8)));
	Debug(_log, "HighestGray: %s", QSTRING_CSTR(colorToQStr(capColors::HighestGray)));
	Debug(_log, "White: %s", QSTRING_CSTR(colorToQStr(capColors::White)));
	Debug(_log, "------------------------------------------------------------------");
	Debug(_log, "");
}

void LutCalibrator::displayPostCalibrationInfo()
{
	Debug(_log, "");
	Debug(_log, "-------------------- Calibrated colors starts --------------------");
	Debug(_log, "Red: %s", QSTRING_CSTR(calColorToQStr(capColors::Red)));
	Debug(_log, "Green: %s", QSTRING_CSTR(calColorToQStr(capColors::Green)));
	Debug(_log, "Blue: %s", QSTRING_CSTR(calColorToQStr(capColors::Blue)));
	Debug(_log, "Yellow: %s", QSTRING_CSTR(calColorToQStr(capColors::Yellow)));
	Debug(_log, "Magenta: %s", QSTRING_CSTR(calColorToQStr(capColors::Magenta)));
	Debug(_log, "Cyan: %s", QSTRING_CSTR(calColorToQStr(capColors::Cyan)));
	Debug(_log, "Orange: %s", QSTRING_CSTR(calColorToQStr(capColors::Orange)));
	Debug(_log, "Pink: %s", QSTRING_CSTR(calColorToQStr(capColors::Pink)));
	Debug(_log, "Azure: %s", QSTRING_CSTR(calColorToQStr(capColors::Azure)));
	Debug(_log, "Brown: %s", QSTRING_CSTR(calColorToQStr(capColors::Brown)));
	Debug(_log, "Purple: %s", QSTRING_CSTR(calColorToQStr(capColors::Purple)));
	Debug(_log, "Low red: %s", QSTRING_CSTR(calColorToQStr(capColors::LowRed)));
	Debug(_log, "Low green: %s", QSTRING_CSTR(calColorToQStr(capColors::LowGreen)));
	Debug(_log, "Low blue: %s", QSTRING_CSTR(calColorToQStr(capColors::LowBlue)));
	Debug(_log, "LowestGray: %s", QSTRING_CSTR(calColorToQStr(capColors::LowestGray)));
	Debug(_log, "Gray1: %s", QSTRING_CSTR(calColorToQStr(capColors::Gray1)));
	Debug(_log, "Gray2: %s", QSTRING_CSTR(calColorToQStr(capColors::Gray2)));
	Debug(_log, "Gray3: %s", QSTRING_CSTR(calColorToQStr(capColors::Gray3)));
	Debug(_log, "Gray4: %s", QSTRING_CSTR(calColorToQStr(capColors::Gray4)));
	Debug(_log, "Gray5: %s", QSTRING_CSTR(calColorToQStr(capColors::Gray5)));
	Debug(_log, "Gray6: %s", QSTRING_CSTR(calColorToQStr(capColors::Gray6)));
	Debug(_log, "Gray7: %s", QSTRING_CSTR(calColorToQStr(capColors::Gray7)));
	Debug(_log, "Gray8: %s", QSTRING_CSTR(calColorToQStr(capColors::Gray8)));
	Debug(_log, "HighestGray: %s", QSTRING_CSTR(calColorToQStr(capColors::HighestGray)));
	Debug(_log, "White: %s", QSTRING_CSTR(calColorToQStr(capColors::White)));
	Debug(_log, "------------------------------------------------------------------");
	Debug(_log, "");
}

bool LutCalibrator::correctionEnd()
{

	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &LutCalibrator::setVideoImage);
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &LutCalibrator::setSystemImage);
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &LutCalibrator::signalSetGlobalImageHandler);

	double floor = qMax(_minColor.red, qMax(_minColor.green, _minColor.blue));
	double ceiling = qMin(_maxColor.red, qMin(_maxColor.green, _maxColor.blue));
	double range = 0;
	double scale = (ceiling - floor) / (161.0 / 255.0);
	int strategy = 0;
	int whiteIndex = capColors::White;


	for (int j = 0; j < (int)(sizeof(_colorBalance) / sizeof(ColorStat)); j++)
		_colorBalance[j].calculateFinalColor();;

	// YUV check
	if (floor >= 2 && !_limitedRange)
	{
		Warning(_log, "YUV limited range detected (black level: %f, ceiling: %f). Restarting the calibration using limited range YUV.", floor, ceiling);

		QJsonObject report;
		report["limited"] = 1;
		SignalLutCalibrationUpdated(report);

		return false;
	}

	// coef autodetection
	int lastCoef = (sizeof(_coefsResult) / sizeof(double)) - 1;
	bool finished = (_coefsResult[lastCoef] != 0);

	if (!finished)
	{
		int nextIndex = _currentCoef;

		_coefsResult[_currentCoef] = fineTune(range, scale, whiteIndex, strategy);

		// choose best
		if (_currentCoef == lastCoef)
		{
			for (int i = 0, best = INT_MAX; i < (int)(sizeof(_coefsResult) / sizeof(double)); i++)
			{
				Debug(_log, "Mean error for %s is: %f", REC(i), _coefsResult[i]);
				if (_coefsResult[i] < best)
				{
					best = _coefsResult[i];
					nextIndex = i;
				}
			}
			Warning(_log, "Best coef is: %s", REC(nextIndex));
		}
		else
			nextIndex = _currentCoef + 1;

		// request next if needed
		if (nextIndex != _currentCoef)
		{
			Warning(_log, "Requesting next coef for switching: %s", REC(nextIndex));

			QJsonObject report;
			report["coef"] = nextIndex;
			SignalLutCalibrationUpdated(report);

			return false;
		}
	}
	else
		_coefsResult[_currentCoef] = fineTune(range, scale, whiteIndex, strategy);



	// display precalibrated colors
	displayPreCalibrationInfo();

	// display stats
	ColorStat whiteBalance = _colorBalance[whiteIndex];

	Debug(_log, "Optimal PQ multi => %f, strategy => %i, white index => %i", range, strategy, whiteIndex);
	Debug(_log, "White correction: (%f, %f, %f)", whiteBalance.scaledRed, whiteBalance.scaledGreen, whiteBalance.scaledBlue);
	Debug(_log, "Min RGB floor: %f, max RGB ceiling: %f, scale: %f", floor, ceiling, scale);
	Debug(_log, "Min RGB range => %s", STRING_CSTR(_minColor));
	Debug(_log, "Max RGB range => %s", STRING_CSTR(_maxColor));
	Debug(_log, "YUV range: %s", (floor >= 2 || _limitedRange) ? "LIMITED" : "FULL");
	Debug(_log, "YUV coefs: %s", REC(_currentCoef));

	// build LUT table
	for (int g = 0; g <= 255; g++)
		for (int b = 0; b <= 255; b++)
			for (int r = 0; r <= 255; r++)
			{
				double Ri = clampDouble((r * whiteBalance.scaledRed - floor) / scale, 0, 1.0);
				double Gi = clampDouble((g * whiteBalance.scaledGreen - floor) / scale, 0, 1.0);
				double Bi = clampDouble((b * whiteBalance.scaledBlue - floor) /scale, 0, 1.0);

				// ootf
				if (strategy == 1)
				{
					Ri = ootf(Ri);
					Gi = ootf(Gi);
					Bi = ootf(Bi);
				}

				// eotf
				if (strategy == 0 || strategy == 1)
				{
					Ri = eotf(range, Ri);
					Gi = eotf(range, Gi);
					Bi = eotf(range, Bi);
				}

				// bt2020
				if (strategy == 0 || strategy == 1)
				{
					fromBT2020toBT709(Ri, Gi, Bi, Ri, Gi, Bi);
				}

				// ootf
				if (strategy == 0 || strategy == 1)
				{
					Ri = ootf(Ri);
					Gi = ootf(Gi);
					Bi = ootf(Bi);
				}

				double finalR = clampDouble(Ri, 0, 1.0);
				double finalG = clampDouble(Gi, 0, 1.0);
				double finalB = clampDouble(Bi, 0, 1.0);

				balanceGray(r, g, b, finalR, finalG, finalB);

				if (_saturation != 1.0 || _luminance != 1.0)
					colorCorrection(finalR, finalG, finalB);

				if (_gammaR != 1.0)
					finalR = std::pow(finalR, 1 / _gammaR);

				if (_gammaG != 1.0)
					finalG = std::pow(finalG, 1 / _gammaG);

				if (_gammaB != 1.0)
					finalB = std::pow(finalB, 1 / _gammaB);

				// save it
				uint32_t ind_lutd = LUT_INDEX(r, g, b);
				_lut.data()[ind_lutd    ] = clampToInt(((finalR) * 255), 0, 255);
				_lut.data()[ind_lutd + 1] = clampToInt(((finalG) * 255), 0, 255);
				_lut.data()[ind_lutd + 2] = clampToInt(((finalB) * 255), 0, 255);
			}

	// display final colors
	displayPostCalibrationInfo();

	return true;
}

void LutCalibrator::applyFilter()
{
	uint8_t* _secondBuffer = &(_lut.data()[LUT_FILE_SIZE]);

	memset(_secondBuffer, 0, LUT_FILE_SIZE);

	for (int r = 0; r < 256; r++)
		for (int g = 0; g < 256; g++)
			for (int b = 0; b < 256; b++)
			{
				uint32_t avR = 0, avG = 0, avB = 0, avCount = 0;
				uint32_t index = LUT_INDEX(r, g, b);

				for (int x = -1; x <= 1; x++)
					for (int y = -1; y <= 1; y++)
						for (int z = -1; z <= 1; z++)
						{
							int X = r + x;
							int Y = g + y;
							int Z = b + z;

							if (X >= 0 && X <= 255 && Y >= 0 && Y <= 255 && Z >= 0 && Z <= 255)
							{
								uint32_t ind = LUT_INDEX(X, Y, Z);
								uint32_t scale = (x == 0 && y == 0 && z == 0) ? 13 : 1;

								uint32_t R = _lut.data()[ind];
								uint32_t G = _lut.data()[ind + 1];
								uint32_t B = _lut.data()[ind + 2];

								if (scale != 1 && R == G && G == B)
								{
									avR = avG = avB = avCount = 0;
									x = y = z = SHRT_MAX;
								}

								avR += R * scale;
								avG += G * scale;
								avB += B * scale;
								avCount += scale;
							}
						}

				_secondBuffer[index] = clampToInt(((avR / (double)avCount)), 0, 255);
				_secondBuffer[index + 1] = clampToInt(((avG / (double)avCount)), 0, 255);
				_secondBuffer[index + 2] = clampToInt(((avB / (double)avCount)), 0, 255);
			}

	memcpy(_lut.data(), _secondBuffer, LUT_FILE_SIZE);
}

double LutCalibrator::fineTune(double& optimalRange, double& optimalScale, int& optimalWhite, int& optimalStrategy)
{
	QString optimalColor;
	//double floor = qMax(_minColor.red, qMax(_minColor.green, _minColor.blue));
	double ceiling = qMin(_maxColor.red, qMin(_maxColor.green, _maxColor.blue));
	capColors primaries[] = { capColors::HighestGray, capColors::LowestGray, capColors::Red, capColors::Green, capColors::Blue, capColors::LowRed, capColors::LowGreen, capColors::LowBlue, capColors::Yellow, capColors::Magenta, capColors::Cyan, capColors::Pink, capColors::Orange, capColors::Azure, capColors::Brown, capColors::Purple,
						capColors::Gray1, capColors::Gray2, capColors::Gray3, capColors::Gray4, capColors::Gray5,  capColors::Gray6, capColors::Gray7, capColors::Gray8, capColors::White };

	double maxError = (double)LLONG_MAX;

	optimalStrategy = 2;
	optimalWhite = capColors::White;
	optimalRange = ceiling;

	double rangeStart = 20, rangeLimit = 150;
	bool restart = false;

	for (int whiteIndex = capColors::Gray1; whiteIndex <= capColors::White; whiteIndex++)
	{
		ColorStat whiteBalance = _colorBalance[whiteIndex];

		for (int scale = (qRound(ceiling) / 8) * 8, limitScale = 512; scale <= limitScale; scale = (scale == limitScale) ? limitScale + 1 : qMin(scale + 4, limitScale))
			for (int strategy = 0; strategy < 3; strategy++)
				for (double range = rangeStart; range <= rangeLimit; range += (range < 5) ? 0.1 : 0.5)
					if (strategy != 2 || range == rangeLimit)
					{
						double currentError = 0;
						QList<QString> colors;
						double lR = -1, lG = -1, lB = -1;

						for (int ind : primaries)
						{
							ColorStat calculated, normalized = _colorBalance[ind];
							normalized /= (double)scale;

							normalized.red *= whiteBalance.scaledRed;
							normalized.green *= whiteBalance.scaledGreen;
							normalized.blue *= whiteBalance.scaledBlue;

							// ootf
							if (strategy == 1)
							{
								normalized.red = ootf(normalized.red);
								normalized.green = ootf(normalized.green);
								normalized.blue = ootf(normalized.blue);
							}

							// eotf
							if (strategy == 0 || strategy == 1)
							{
								normalized.red = eotf(range, normalized.red);
								normalized.green = eotf(range, normalized.green);
								normalized.blue = eotf(range, normalized.blue);
							}

							// bt2020
							if (strategy == 0 || strategy == 1)
							{
								fromBT2020toBT709(normalized.red, normalized.green, normalized.blue, calculated.red, calculated.green, calculated.blue);
							}

							// ootf
							if (strategy == 0 || strategy == 1)
							{
								calculated.red = ootf(calculated.red);
								calculated.green = ootf(calculated.green);
								calculated.blue = ootf(calculated.blue);
							}
							else
							{
								calculated.red = normalized.red;
								calculated.green = normalized.green;
								calculated.blue = normalized.blue;
							}

							calculated.red = clampDouble(calculated.red, 0, 1.0) * 255.0;
							calculated.green = clampDouble(calculated.green, 0, 1.0) * 255.0;
							calculated.blue = clampDouble(calculated.blue, 0, 1.0) * 255.0;

							if ((ind != capColors::HighestGray ||
									((calculated.red <= 250.0 && calculated.green <= 250.0 && calculated.blue <= 250.0) && (calculated.red >= 200.0 && calculated.green >= 200.0 && calculated.blue >= 200.0)))  &&
								(ind != capColors::LowestGray ||
									((calculated.red >= 4 && calculated.green >= 4 && calculated.blue >= 4) && (calculated.red <= 28 && calculated.green <= 28 && calculated.blue <= 28))))
							{
								if (ind == capColors::LowRed)
									lR = calculated.red;
								if (ind == capColors::LowGreen)
									lG = calculated.green;
								if (ind == capColors::LowBlue)
									lB = calculated.blue;

								currentError += getError(primeColors[ind], calculated);
								colors.push_back(calculated.toQString());
							}
							else
							{
								currentError = maxError +1;
								break;
							}
						}

						if (lR >= 0 && lG >= 0 && lB >= 0)
						{
							double m = qMax(lR, qMax(lG, lB));
							double n = qMin(lR, qMin(lG, lB));
							currentError += 8 * std::pow(m - n, 2);
						}

						if (maxError > currentError)
						{
							maxError = currentError;
							optimalRange = range;
							optimalStrategy = strategy;
							optimalScale = scale;
							optimalWhite = whiteIndex;
							optimalColor = "";
							for (auto c : colors)
								optimalColor += QString("%1 ,").arg(c);
							optimalColor += QString(" range: %1, strategy: %2, scale: %3, white: %4, error: %5").arg(optimalRange).arg(optimalStrategy).arg(optimalScale).arg(optimalWhite).arg(currentError);
						}
					}

		if (whiteIndex == capColors::White && maxError == LLONG_MAX && !restart)
		{
			restart = true;
			whiteIndex = capColors::LowestGray;
			rangeStart = 0.1;
			rangeLimit = 20;
			Debug(_log, "Restarting calculation");
		}
	}

	Debug(_log, "Best result => %s", QSTRING_CSTR(optimalColor));

	return maxError + 1;
}

bool LutCalibrator::finalize(bool fastTrack)
{
	QString fileName = QString("%1%2").arg(_rootPath).arg("/lut_lin_tables.3d");
	QFile file(fileName);

	bool ok = true;

	if (!fastTrack)
	{
		disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &LutCalibrator::setVideoImage);
		disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &LutCalibrator::setSystemImage);
		disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &LutCalibrator::signalSetGlobalImageHandler);
	}

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		Error(_log, "Could not open: %s for writing (read-only file system or lack of rights)", QSTRING_CSTR(fileName));
		ok = false;
	}
	else
	{
		double floor = qMax(_minColor.red, qMax(_minColor.green, _minColor.blue));
		double ceil = qMin(_maxColor.red, qMin(_maxColor.green, _maxColor.blue));
		double delta = ceil - floor;
		double Kr = _coefs[_currentCoef].red, Kg = _coefs[_currentCoef].green, Kb = _coefs[_currentCoef].blue;

		// RGB HDR and INTRO
		Debug(_log, "----------------- Preparing and saving LUT table --------------------");
		Debug(_log, "Initial mode: %s", (fastTrack) ? "YES" : "NO");
		Debug(_log, "Using YUV coefs: %s", REC(_currentCoef));
		Debug(_log, "YUV table range: %s", (_limitedRange) ? "LIMITED" : "FULL");

		if (floor <= ceil)
		{
			Debug(_log, "Min RGB floor: %f, max RGB ceiling: %f", floor, ceil);
			Debug(_log, "Delta RGB range => %f", delta);
			Debug(_log, "Min RGB range => %s", STRING_CSTR(_minColor));
			Debug(_log, "Max RGB range => %s", STRING_CSTR(_maxColor));
		}

		file.write((const char*)_lut.data(), LUT_FILE_SIZE);
		Debug(_log, "LUT RGB HDR table (1/3) is ready");

		// YUV HDR		
		uint8_t* _yuvBuffer = &(_lut.data()[LUT_FILE_SIZE]);

		memset(_yuvBuffer, 0, LUT_FILE_SIZE);
		for (int y = 0; y < 256 && !fastTrack; y++)
			for (int u = 0; u < 256; u++)
				for (int v = 0; v < 256; v++)
				{
					double r, g, b;

					if (_limitedRange)
					{
						r = (255.0 / 219.0) * y + (255.0 / 112) * v * (1 - Kr) - (255.0 * 16.0 / 219 + 255.0 * 128.0 / 112.0 * (1 - Kr));
						g = (255.0 / 219.0) * y - (255.0 / 112) * u * (1 - Kb) * Kb / Kg - (255.0 / 112.0) * v * (1 - Kr) * Kr / Kg
							- (255.0 * 16.0 / 219.0 - 255.0 / 112.0 * 128.0 * (1 - Kb) * Kb / Kg - 255.0 / 112.0 * 128.0 * (1 - Kr) * Kr / Kg);
						b = (255.0 / 219.0) * y + (255.0 / 112.0) * u * (1 - Kb) - (255.0 * 16 / 219.0 + 255.0 * 128.0 / 112.0 * (1 - Kb));
					}
					else
					{
						r = y + 2 * (v - 128) * (1 - Kr);
						g = y - 2 * (u - 128) * (1 - Kb) * Kb / Kg - 2 * (v - 128) * (1 - Kr) * Kr / Kg;
						b = y + 2 * (u - 128) * (1 - Kb);
					}

					int _R = clampToInt(r, 0, 255);
					int _G = clampToInt(g, 0, 255);
					int _B = clampToInt(b, 0, 255);

					uint32_t indexRgb = LUT_INDEX(_R, _G, _B);
					uint32_t index = LUT_INDEX(y, u, v);

					_yuvBuffer[index] = _lut.data()[indexRgb];
					_yuvBuffer[index + 1] = _lut.data()[indexRgb + 1];
					_yuvBuffer[index + 2] = _lut.data()[indexRgb + 2];
				}
		file.write((const char*)_yuvBuffer, LUT_FILE_SIZE);
		Debug(_log, "LUT YUV HDR table (2/3) is ready");

		// YUV
		for (int y = 0; y < 256; y++)
			for (int u = 0; u < 256; u++)
				for (int v = 0; v < 256; v++)
				{
					uint32_t ind_lutd = LUT_INDEX(y, u, v);
					double r, g, b;

					if (_limitedRange)
					{
						r = (255.0 / 219.0) * y + (255.0 / 112) * v * (1 - Kr) - (255.0 * 16.0 / 219 + 255.0 * 128.0 / 112.0 * (1 - Kr));
						g = (255.0 / 219.0) * y - (255.0 / 112) * u * (1 - Kb) * Kb / Kg - (255.0 / 112.0) * v * (1 - Kr) * Kr / Kg
							- (255.0 * 16.0 / 219.0 - 255.0 / 112.0 * 128.0 * (1 - Kb) * Kb / Kg - 255.0 / 112.0 * 128.0 * (1 - Kr) * Kr / Kg);
						b = (255.0 / 219.0) * y + (255.0 / 112.0) * u * (1 - Kb) - (255.0 * 16 / 219.0 + 255.0 * 128.0 / 112.0 * (1 - Kb));
					}
					else
					{
						r = y + 2 * (v - 128) * (1 - Kr);
						g = y - 2 * (u - 128) * (1 - Kb) * Kb / Kg - 2 * (v - 128) * (1 - Kr) * Kr / Kg;
						b = y + 2 * (u - 128) * (1 - Kb);
					}

					_lut.data()[ind_lutd] = clampToInt(r, 0, 255);
					_lut.data()[ind_lutd + 1] = clampToInt(g, 0, 255);
					_lut.data()[ind_lutd + 2] = clampToInt(b, 0, 255);

				}
		file.write((const char*)_lut.data(), LUT_FILE_SIZE);

		if (_mjpegCalibration && fastTrack)
		{
			file.seek(LUT_FILE_SIZE);
			file.write((const char*)_lut.data(), LUT_FILE_SIZE);
		}

		file.flush();
		Debug(_log, "LUT YUV table (3/3) is ready");

		// finish
		file.close();
		Debug(_log, "Your new LUT file is saved as %s. Is ready for usage: %s.", QSTRING_CSTR(fileName), (fastTrack) ? "NO. It's temporary LUT table without HDR information." : "YES");
		Debug(_log, "---------------------- LUT table is saved -----------------------");
		Debug(_log, "");
	}

	if (!fastTrack)
		stopHandler();

	return ok;
}
