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

namespace ColorSpaceMath {
	enum HDR_GAMMA { PQ = 0, HLG, sRGB, BT2020inSRGB };
}

struct BestResult;

class LutCalibrator : public QObject
{
	Q_OBJECT

public:
	LutCalibrator();

signals:
	void SignalLutCalibrationUpdated(const QJsonObject& data);

public slots:
	void startHandler(QString rootpath, hyperhdr::Components defaultComp, bool debug, bool postprocessing);
	void stopHandler();
	void setVideoImage(const QString& name, const Image<ColorRgb>& image);
	void setSystemImage(const QString& name, const Image<ColorRgb>& image);
	void signalSetGlobalImageHandler(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin);
	void calibrate();

private:
	void fineTune();
	void printReport();
	QString generateReport(bool full);
	void sendReport(QString report);
	bool set1to1LUT();
	void notifyCalibrationFinished();
	void notifyCalibrationMessage(QString message, bool started = false);
	void error(QString message);
	void handleImage(const Image<ColorRgb>& image);
	void calibration();
	void setupWhitePointCorrection();
	bool setTestData();

	void capturedPrimariesCorrection(ColorSpaceMath::HDR_GAMMA gamma, double gammaHLG, double nits, int coef, linalg::mat<double, 3, 3>& convert_bt2020_to_XYZ, linalg::mat<double, 3, 3>& convert_XYZ_to_corrected, bool printDebug = false);
	bool parseTextLut2binary(const char* filename = "D:/interpolated_lut.txt", const char* outfile = "D:/lut_lin_tables.3d");

	static QString writeLUT(Logger* _log, QString _rootPath, BestResult* bestResult, std::vector<std::vector<std::vector<CapturedColor>>>* all);

	Logger* _log;
	std::shared_ptr<BoardUtils::CapturedColors> _capturedColors;
	std::shared_ptr<YuvConverter> _yuvConverter;
	std::shared_ptr< BestResult> bestResult;
	MemoryBuffer<uint8_t> _lut;
	QString _rootPath;
	bool	_debug;
	bool	_postprocessing;
};
