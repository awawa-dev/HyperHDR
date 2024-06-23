#pragma once

/* LutCalibrator.h
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
	#include <QObject>
	#include <QList>
	#include <QDateTime>
	#include <QJsonObject>
	#include <QString>

	#include <algorithm>
#endif

#include <image/MemoryBuffer.h>
#include <image/Image.h>
#include <utils/Components.h>

class Logger;
class GrabberWrapper;

class LutCalibrator : public QObject
{
	Q_OBJECT

private:
	static LutCalibrator* instance;
	struct ColorStat
	{
		double	red = 0, green = 0, blue = 0, count = 0, scaledRed = 1, scaledGreen = 1, scaledBlue = 1;

		ColorStat() = default;

		ColorStat(double r, double g, double b)
		{
			red = r;
			green = g;
			blue = b;
		}

		void calculateFinalColor()
		{
			red = red / count;
			green = green / count;
			blue = blue / count;

			if (red > 1 && green > 1  && blue > 1)
			{
				double scale = qMax(red, qMax(green, blue));
				scaledRed = scale / red;
				scaledGreen = scale / green;
				scaledBlue = scale / blue;
			}
		}

		void reset()
		{
			red = 0;
			green = 0;
			blue = 0;
			count = 0;
			scaledRed = 0;
			scaledGreen = 0;
			scaledBlue = 0;
		}		

		void AddColor(ColorRgb y)
		{
			red += y.red;
			green += y.green;
			blue += y.blue;
			count++;
		}

		ColorStat& operator/=(const double x)
		{
			this->red /= x;
			this->green /= x;
			this->blue /= x;
			return *this;
		}

		QString toQString()
		{
			return QString("(%1, %2, %3)").arg(red).arg(green).arg(blue);
		}
	};

	enum capColors { Red = 0, Green = 1, Blue = 2, Yellow = 3, Magenta = 4, Cyan = 5, Orange = 6, Pink = 7, Azure = 8, Brown = 9, Purple = 10, LowRed = 11, LowGreen = 12, LowBlue = 13, LowestGray = 14,
					 Gray1 = 15, Gray2 = 16, Gray3 = 17, Gray4 = 18, Gray5 = 19, Gray6 = 20, Gray7 = 21, Gray8 = 22, HighestGray = 23, White = 24, None = 25 };

public:
	LutCalibrator();
	~LutCalibrator();

signals:
	void SignalLutCalibrationUpdated(const QJsonObject& data);

public slots:
	void incomingCommand(QString rootpath, GrabberWrapper* grabberWrapper, hyperhdr::Components defaultComp, int checksum, ColorRgb startColor, ColorRgb endColor, bool limitedRange, double saturation, double luminance, double gammaR, double gammaG, double gammaB, int coef);
	void stopHandler();
	void setVideoImage(const QString& name, const Image<ColorRgb>& image);
	void setSystemImage(const QString& name, const Image<ColorRgb>& image);
	void signalSetGlobalImageHandler(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin);

private:
	void handleImage(const Image<ColorRgb>& image);
	bool increaseColor(ColorRgb& color);
	void storeColor(const ColorRgb& inputColor, const ColorRgb& color);
	bool finalize(bool fastTrack = false);
	bool correctionEnd();

	inline int clampInt(int val, int min, int max) { return qMin(qMax(val, min), max);}
	inline double clampDouble(double val, double min, double max) { return qMin(qMax(val, min), max); }
	inline int clampToInt(double val, int min, int max) { return qMin(qMax(qRound(val), min), max); }

	double	eotf(double scale, double  x) noexcept;
	double	inverse_eotf(double x) noexcept;
	double	ootf(double  v) noexcept;
	double	inverse_gamma(double x) noexcept;
	void	colorCorrection(double& r, double& g, double& b);
	void	balanceGray(int r, int g, int b, double& _r, double& _g, double& _b);
	void	fromBT2020toXYZ(double r, double g, double b, double& x, double& y, double& z);
	void	fromXYZtoBT709(double x, double y, double z, double& r, double& g, double& b);
	void	fromBT2020toBT709(double x, double y, double z, double& r, double& g, double& b);
	void	toneMapping(double xhdr, double yhdr, double zhdr, double& xsdr, double& ysdr, double& zsdr);
	QString colorToQStr(capColors index);
	QString colorToQStr(ColorRgb color);
	QString calColorToQStr(capColors index);
	void	displayPreCalibrationInfo();
	void	displayPostCalibrationInfo();
	double	fineTune(double& optimalRange, double& optimalScale, int& optimalWhite, int& optimalStrategy);
	double	getError(ColorRgb first, ColorStat second);
	void	applyFilter();

	Logger* _log;
	bool	_mjpegCalibration;
	bool	_finish;
	bool	_limitedRange;
	int		_checksum;
	int		_currentCoef;
	double	_coefsResult[3];
	int		_warningCRC;
	int		_warningMismatch;
	double	_saturation;
	double	_luminance;
	double	_gammaR;
	double	_gammaG;
	double	_gammaB;
	qint64	_timeStamp;
	ColorRgb _startColor;
	ColorRgb _endColor;
	ColorRgb _minColor;
	ColorRgb _maxColor;
	ColorStat _colorBalance[26];
	MemoryBuffer<uint8_t> _lut;
	QString _rootPath;

	static ColorRgb primeColors[];

	// Color coefs YUV to RGB: http://avisynth.nl/index.php/Color_conversions
	// FCC, Rec.709, Rec.601 coefficients 
	ColorStat _coefs[3] = { ColorStat(0.3, 0.59, 0.11), ColorStat(0.2126, 0.7152, 0.0722), ColorStat(0.299, 0.587, 0.114)};
};
