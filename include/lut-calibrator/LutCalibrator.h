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
class YuvConverter;
class CapturedColor;

namespace BoardUtils
{
	class CapturedColors;
};

namespace linalg {
	template<class T, int M, int N> struct mat;
	template<class T, int M> struct vec;
}

class LutCalibrator : public QObject
{
	Q_OBJECT

public:
	LutCalibrator();

signals:
	void SignalLutCalibrationUpdated(const QJsonObject& data);

public slots:
	void incomingCommand(QString rootpath, GrabberWrapper* grabberWrapper, hyperhdr::Components defaultComp, int checksum, double saturation, double luminance, double gammaR, double gammaG, double gammaB);
	void stopHandler();
	void setVideoImage(const QString& name, const Image<ColorRgb>& image);
	void setSystemImage(const QString& name, const Image<ColorRgb>& image);
	void signalSetGlobalImageHandler(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin);
	void calibrate();

private:
	void toneMapping();
	QString generateReport(bool full);
	void sendReport(QString report);
	bool set1to1LUT();
	void notifyCalibrationFinished();
	void notifyCalibrationMessage(QString message, bool started = false);
	void error(QString message);
	void handleImage(const Image<ColorRgb>& image);
	linalg::vec<double, 3> hdr_to_srgb(linalg::vec<double, 3> yuv, const linalg::vec<uint8_t, 2>& UV, const linalg::vec<double, 3>& aspect, const linalg::mat<double, 4, 4>& coefMatrix, int nits, bool altConvert, const linalg::mat<double, 3, 3>& bt2020_to_sRgb, bool tryBt2020Range);
	void scoreBoard(bool testOnly, const linalg::mat<double, 4, 4>& coefMatrix, int nits, linalg::vec<double, 3> aspect, bool tryBt2020Range, bool altConvert, const linalg::mat<double, 3, 3>& bt2020_to_sRgb, const double& minError, double& currentError);
	void tryHDR10();
	void setupWhitePointCorrection();
	bool finalize(bool fastTrack = false);

	inline int clampInt(int val, int min, int max) { return qMin(qMax(val, min), max);}
	inline double clampDouble(double val, double min, double max) { return qMin(qMax(val, min), max); }
	inline int clampToInt(double val, int min, int max) { return qMin(qMax(qRound(val), min), max); }

	double	fineTune(double& optimalRange, double& optimalScale, int& optimalWhite, int& optimalStrategy);
	double	getError(const linalg::vec<uint8_t, 3>& first, const linalg::vec<uint8_t, 3>& second);
	void	capturedPrimariesCorrection(double nits, int coef, linalg::mat<double, 3, 3>& convert_bt2020_to_XYZ, linalg::mat<double, 3, 3>& convert_XYZ_to_corrected);
	bool	parseTextLut2binary(const char* filename = "D:/interpolated_lut.txt", const char* outfile = "D:/lut_lin_tables.3d");

	Logger* _log;
	std::shared_ptr<BoardUtils::CapturedColors> _capturedColors;
	std::shared_ptr<YuvConverter> _yuvConverter;
	MemoryBuffer<uint8_t> _lut;
	QString _rootPath;	
};
