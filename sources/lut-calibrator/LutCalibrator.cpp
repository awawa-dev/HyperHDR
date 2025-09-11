 /* LutCalibrator.cpp
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
	#include <QJsonArray>
	#include <QJsonObject>
	#include <QFile>
	#include <QDateTime>
	#include <QThread>

	#include <cmath>
	#include <cfloat>
	#include <climits>	
#endif

#define STRING_CSTR(x) (x.operator std::string()).c_str()

#include <QCoreApplication>
#include <QRunnable>
#include <QThreadPool>
#include <utils/Logger.h>
#include <lut-calibrator/LutCalibrator.h>
#include <utils/GlobalSignals.h>
#include <base/GrabberWrapper.h>
#include <api/HyperAPI.h>
#include <base/HyperHdrManager.h>
#include <infinite-color-engine/ColorSpace.h>
#include <infinite-color-engine/YuvConverter.h>
#include <lut-calibrator/BoardUtils.h>
#include <utils-image/utils-image.h>
#include <linalg.h>
#include <fstream>
#include <lut-calibrator/CalibrationWorker.h>
#include <infinite-color-engine/InfiniteRgbInterpolator.h>
#include <infinite-color-engine/InfiniteYuvInterpolator.h>
#include <infinite-color-engine/InfiniteHybridInterpolator.h>
#include <infinite-color-engine/InfiniteStepperInterpolator.h>
#include <infinite-color-engine/InfiniteProcessing.h>

using namespace linalg;
using namespace aliases;
using namespace ColorSpaceMath;
using namespace BoardUtils;


#define LUT_FILE_SIZE 50331648
#define LUT_INDEX(y,u,v) ((y + (u<<8) + (v<<16))*3)

/////////////////////////////////////////////////////////////////
//                          HELPERS                            //
/////////////////////////////////////////////////////////////////
struct MappingPrime;


enum SampleColor { RED = 0, GREEN, BLUE, LOW_RED, LOW_GREEN, LOW_BLUE };

class CreateLutWorker : public QRunnable
{
	const int startV;
	const int endV;
	const int phase;
	const YuvConverter* yuvConverter;
	const BestResult* bestResult;
	uint8_t* lut;
public:
	CreateLutWorker(int _startV, int _endV, int _phase, YuvConverter* _yuvConverter, BestResult* _bestResult, uint8_t* _lut) :
		startV(_startV),
		endV(_endV),
		phase(_phase),
		yuvConverter(_yuvConverter),
		bestResult(_bestResult),
		lut(_lut)
	{
	};
	void run() override;
};

class DefaultLutCreatorWorker : public QRunnable
{
	Logger* log;
	BestResult bestResult;
	QString path;
public:
	DefaultLutCreatorWorker(BestResult& _bestResult,QString _path) :
		log(Logger::getInstance("LUT_CREATOR")),
		bestResult(_bestResult),
		path(_path)
	{
		setAutoDelete(true);
	};
	void run() override;
};

/////////////////////////////////////////////////////////////////
//                      LUT CALIBRATOR                         //
/////////////////////////////////////////////////////////////////

LutCalibrator::LutCalibrator(QString rootpath, hyperhdr::Components defaultComp, bool debug, bool lchCorrection)
{
	_log = Logger::getInstance("CALIBRATOR");
	_capturedColors = std::make_shared<CapturedColors>();
	_yuvConverter = std::make_shared<YuvConverter>();

	
	_rootPath = rootpath;
	_debug = debug;
	_lchCorrection = lchCorrection;
	_defaultComp = defaultComp;
	_forcedExit = false;
}

LutCalibrator::~LutCalibrator()
{
	Info(_log, "The calibration object is deleted");
}

static void unpackP010(double *y, double *u, double *v)
{
	if (y !=nullptr)
	{
		double val = FrameDecoderUtils::unpackLuminanceP010(*y);
		
		*y = val;		
	}

	for (auto chroma : { u, v })
		if (chroma != nullptr)
		{			
			double val = (*chroma * 255.0 - 128.0) / 128.0;
			double fVal = FrameDecoderUtils::unpackChromaP010(std::abs(val));
			*chroma = (128.0  + ((val < 0) ? -fVal : fVal) * 112.0) / 255.0;
		}
};

static void unpackP010(double3& yuv)
{
	unpackP010(&yuv.x, &yuv.y, &yuv.z);
};

void LutCalibrator::cancelCalibrationSafe()
{
	_forcedExit = true;
	AUTO_CALL_0(this, stopHandler);
}

void LutCalibrator::error(QString message)
{
	QJsonObject report;
	stopHandler();
	Error(_log, QSTRING_CSTR(message));
	report["error"] = message;
	SignalLutCalibrationUpdated(report);
}

QString LutCalibrator::generateReport(bool full)
{
	const int SCALE = SCREEN_COLOR_DIMENSION - 1;

	const std::list<std::pair<std::string, int3>> reportColors = {
			{ "White",      int3(SCALE, SCALE, SCALE) },
			{ "Red",        int3(SCALE, 0, 0) },
			{ "Green",      int3(0, SCALE, 0) },
			{ "Blue",       int3(0, 0, SCALE) },
			{ "UpperRed",   int3(SCALE * 3/ 4, 0, 0) },
			{ "UpperGreen", int3(0, SCALE * 3/ 4, 0) },
			{ "UpperBlue",  int3(0, 0, SCALE *3 / 4) },
			{ "MiddleRed",  int3(SCALE / 2, 0, 0) },
			{ "MiddleGreen",int3(0, SCALE / 2, 0) },
			{ "MiddleBlue", int3(0, 0, SCALE / 2) },
			{ "LowRed",     int3(SCALE / 4, 0, 0) },
			{ "LowGreen",   int3(0, SCALE / 4, 0) },
			{ "LowBlue",    int3(0, 0, SCALE / 4) },
			{ "Yellow",		int3(SCALE, SCALE, 0) },
			{ "Magenta",	int3(SCALE, 0, SCALE) },
			{ "Cyan",		int3(0, SCALE, SCALE) },
			{ "Orange",		int3(SCALE, SCALE / 2, 0) },
			{ "LimeBlue",	int3(0, SCALE, SCALE / 2) },
			{ "Pink",		int3(SCALE, 0, SCALE / 2) },
			{ "LimeRed",	int3(SCALE / 2, SCALE, 0) },
			{ "Azure",		int3(0, SCALE / 2, SCALE) },
			{ "Violet",		int3(SCALE / 2, 0, SCALE) },
			{ "WHITE_0",	int3(0, 0, 0) },
			{ "WHITE_1",	int3(1, 1, 1) },
			{ "WHITE_2",	int3(2, 2, 2) },
			{ "WHITE_3",	int3(3, 3, 3) },
			{ "WHITE_4",	int3(4, 4, 4) },
			{ "WHITE_5",	int3(5, 5, 5) },
			{ "WHITE_6",	int3(6, 6, 6) },
			{ "WHITE_7",	int3(7, 7, 7) },
			{ "WHITE_8",	int3(8, 8, 8) },
			{ "WHITE_9",	int3(9, 9, 9) },
			{ "WHITE_10",	int3(10, 10, 10) },
			{ "WHITE_11",	int3(11, 11, 11) },
			{ "WHITE_12",	int3(12, 12, 12) },
			{ "WHITE_13",	int3(13, 13, 13) },
			{ "WHITE_14",	int3(14, 14, 14) },
			{ "WHITE_15",	int3(15, 15, 15) },
			{ "WHITE_16",	int3(16, 16, 16) }
	};

	QStringList rep;
	for (const auto& color : reportColors)
		if (color.second.x < SCREEN_COLOR_DIMENSION && color.second.y < SCREEN_COLOR_DIMENSION && color.second.z < SCREEN_COLOR_DIMENSION)
		{
			const auto& testColor = _capturedColors->all[color.second.x][color.second.y][color.second.z];
			

			if (!full)
			{
				auto list = testColor.getInputYUVColors();

				QStringList colors;
				for (auto i = list.begin(); i != list.end(); i++)
				{
					const auto& yuv = *(i);

					auto rgbBT709 = _yuvConverter->toRgb(_capturedColors->getRange(), YuvConverter::BT709, static_cast<double3>(yuv.first) / 255.0) * 255.0;

					colors.append(QString("%1 (YUV: %2)")
						.arg(vecToString(ColorSpaceMath::round_to_0_255<byte3>(rgbBT709)), 12)
						.arg(vecToString(yuv.first), 12)
					);
				}

				rep.append(QString("%1: %2 => %3 %4")
					.arg(QString::fromStdString(color.first), 12)
					.arg(vecToString(testColor.getSourceRGB()), 12)
					.arg(colors.join(", "))
					.arg(((list.size() > 1) ? " [source noise detected]" : "")));
			}
			else
			{				
				auto list = testColor.getFinalRGB();

				QStringList colors;
				for (auto i = list.begin(); i != list.end(); i++)
				{
					colors.append(QString("%1").arg(vecToString((*i)), 12));
				}
				rep.append(QString("%1: %2 => %3 %4")
					.arg(QString::fromStdString(color.first), 12)
					.arg(vecToString(testColor.getSourceRGB()), 12)
					.arg(colors.join(", "))
					.arg(((list.size() > 1) ? "[corrected, source noise detected]" : "[corrected]")));
			}

		};
	return rep.join("\r\n");
}

void LutCalibrator::notifyCalibrationFinished()
{	
	QJsonObject report;
	report["finished"] = true;
	emit SignalLutCalibrationUpdated(report);
}

void LutCalibrator::notifyCalibrationMessage(QString message, bool started)
{
	QJsonObject report;
	report["message"] = message;
	if (started)
	{
		report["start"] = true;
	}
	emit SignalLutCalibrationUpdated(report);
}

bool LutCalibrator::set1to1LUT()
{
	_lut.resize(LUT_FILE_SIZE);

	if (_lut.data() != nullptr)
	{
		for (int y = 0; y < 256; y++)
			for (int u = 0; u < 256; u++)
				for (int v = 0; v < 256; v++)
				{
					uint32_t ind_lutd = LUT_INDEX(y, u, v);
					_lut.data()[ind_lutd] = y;
					_lut.data()[ind_lutd + 1] = u;
					_lut.data()[ind_lutd + 2] = v;
				}

		emit GlobalSignals::getInstance()->SignalSetLut(&_lut);
		QThread::msleep(500);

		return true;
	}

	return false;
}


void LutCalibrator::sendReport(Logger* _log, QString report)
{
	int total = 0;
	QStringList list;

	auto lines = report.split("\r\n");
	for (const auto& line : lines)
	{
		if (total + line.size() + 4 >= 1024)
		{
			Debug(_log, REPORT_TOKEN "\r\n%s", QSTRING_CSTR(list.join("\r\n")));
			total = 4;
			list.clear();
		}

		total += line.size() + 4;
		list.append(line);
	}

	Debug(_log, REPORT_TOKEN "%s\r\n", QSTRING_CSTR(list.join("\r\n")));
}

void LutCalibrator::startHandler()
{
	stopHandler();

	_capturedColors.reset();
	_capturedColors = std::make_shared<CapturedColors>();
	bestResult = std::make_shared<BestResult>();

	if (setTestData())
	{
		notifyCalibrationMessage("Start calibration using test data");
		QTimer::singleShot(1000, this, &LutCalibrator::calibrate);
		return;
	}

	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, false);
	QThread::msleep(1500);
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);
	QThread::msleep(1500);

	if (!set1to1LUT())
	{			
		error("Could not allocated memory (~50MB) for internal temporary buffer. Stopped.");
		return;
	}				

	if (_defaultComp == hyperhdr::COMP_VIDEOGRABBER)
	{
		auto message = "Using video grabber as a source<br/>Waiting for first captured test board..";
		notifyCalibrationMessage(message);
		Debug(_log, message);
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &LutCalibrator::setVideoImage, Qt::ConnectionType::UniqueConnection);
	}
	else if (_defaultComp == hyperhdr::COMP_SYSTEMGRABBER)
	{
		auto message = "Using system grabber as a source<br/>Waiting for first captured test board..";
		notifyCalibrationMessage(message);
		Debug(_log, message);
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &LutCalibrator::setSystemImage, Qt::ConnectionType::UniqueConnection);
	}
	else if (_defaultComp == hyperhdr::COMP_FLATBUFSERVER)
	{
		auto message = "Using flatbuffers/protobuffers as a source<br/>Waiting for first captured test board..";
		notifyCalibrationMessage(message);
		Debug(_log, message);
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &LutCalibrator::signalSetGlobalImageHandler, Qt::ConnectionType::UniqueConnection);
	}
	else
	{
		error(QString("Cannot use '%1' as a video source for calibration").arg(hyperhdr::componentToString(_defaultComp)));
	}
}

void LutCalibrator::stopHandler()
{
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &LutCalibrator::setSystemImage);
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &LutCalibrator::setVideoImage);
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &LutCalibrator::signalSetGlobalImageHandler);
	_lut.releaseMemory();

	if (_forcedExit)
	{
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, false);
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);

		if (_defaultComp == hyperhdr::COMP_VIDEOGRABBER)
		{
			emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_VIDEOGRABBER, -1, true);
		}
		if (_defaultComp == hyperhdr::COMP_FLATBUFSERVER)
		{
			emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_FLATBUFSERVER, -1, true);
		}
	}
}

void LutCalibrator::setVideoImage(const QString& /*name*/, const Image<ColorRgb>& image)
{
	handleImage(image);
}

void LutCalibrator::setSystemImage(const QString& /*name*/, const Image<ColorRgb>& image)
{
	handleImage(image);
}

void LutCalibrator::signalSetGlobalImageHandler(int /*priority*/, const Image<ColorRgb>& image, int /*timeout_ms*/, hyperhdr::Components /*origin*/)
{
	handleImage(image);
}

void LutCalibrator::handleImage(const Image<ColorRgb>& image)
{	
	//////////////////////////////////////////////////////////////////////////
	/////////////////////////  Verify source  ////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	if (image.width() < 1280 || image.height() < 720)
	{
		//image.save(QSTRING_CSTR(QString("d:/testimage_%1_x_%2.yuv").arg(image.width()).arg(image.height())));
		error(QString("Too low resolution: 1280/720 is the minimum. Received video frame: %1x%2. Stopped.").arg(image.width()).arg(image.height()));		
		return;
	}

	if (image.width() * 1080 != image.height() * 1920)
	{
		error("Invalid resolution width/height ratio. Expected aspect: 1920/1080 (or the same 1280/720 etc). Stopped.");
		return;
	}

	auto pixelFormat = image.getOriginFormat();
	if (pixelFormat != PixelFormat::NV12 && pixelFormat != PixelFormat::MJPEG && pixelFormat != PixelFormat::YUYV && pixelFormat != PixelFormat::P010)
	{
		error("Only NV12/MJPEG/YUYV/P010 video format for the USB grabber and NV12 for the flatbuffers source are supported for the LUT calibration.");
		return;
	}
	else if (pixelFormat == PixelFormat::P010)
	{
		bestResult->signal.isSourceP010 = true;
	}

	int boardIndex = -1;

	if (!parseBoard(_log, image, boardIndex, (*_capturedColors.get()), true) || _capturedColors->isCaptured(boardIndex))
	{		
		return;
	}
	
	_capturedColors->setCaptured(boardIndex);

	notifyCalibrationMessage(QString("Captured test board: %1<br/>Waiting for the next one...").arg(boardIndex));


	if (_capturedColors->areAllCaptured())
	{
		Info(_log, "All boards are captured. Starting calibration...");
		stopHandler();
		notifyCalibrationMessage(QString("All boards are captured<br/>Processing...<br/>This will take a lot of time"), true);
		QTimer::singleShot(200, this, &LutCalibrator::calibrate);
	}
}


struct MappingPrime {
	byte3 prime{};
	CapturedColor* sample = nullptr;
	double3 org{};
	double3 real{};
	double3 delta{};
};
/*
double3 acesToneMapping(double3 input)
{
	const double3x3 aces_input_matrix =
	{
		{0.59719f, 0.35458f, 0.04823f},
		{0.07600f, 0.90834f, 0.01566f},
		{0.02840f, 0.13383f, 0.83777f}
	};

	const double3x3 aces_output_matrix =
	{
		{1.60475f, -0.53108f, -0.07367f},
		{-0.10208f,  1.10813f, -0.00605f},
		{-0.00327f, -0.07276f,  1.07602f}
	};

	auto rtt_and_odt_fit = [](double3 v)
		{
			double3 a = v * (v + 0.0245786) - 0.000090537;
			double3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
			return a / b;
		};

	input = mul(aces_input_matrix, input);
	input = rtt_and_odt_fit(input);
	return mul(aces_output_matrix, input);
}

double3 uncharted2_filmic(double3 v)
{
	float exposure_bias = 2.0f;

	auto uncharted2_tonemap_partial = [](double3 x)
		{
			float A = 0.15f;
			float B = 0.50f;
			float C = 0.10f;
			float D = 0.20f;
			float E = 0.02f;
			float F = 0.30f;
			return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
		};

	double3 curr = uncharted2_tonemap_partial(v * exposure_bias);

	double3 W = double3(11.2f);
	double3 white_scale = double3(1.0f) / uncharted2_tonemap_partial(W);
	return curr * white_scale;
}*/

static void doToneMapping(const LchLists& m, double3& p)
{
	auto a = xyz_to_lch(from_sRGB_to_XYZ(p) * 100.0);

	if (a.y < 0.1)
		return;

	
	double3 correctionHigh{};
	auto iterHigh = m.high.begin();
	auto lastHigh = *(iterHigh++);
	for (; iterHigh != m.high.end(); lastHigh = *(iterHigh++))
		if ((lastHigh.w >= a.z && a.z >= (*iterHigh).w))
		{
			auto& current = (*iterHigh);
			double lastAsp = lastHigh.w - a.z;
			double curAsp = a.z - current.w;
			double prop = 1 - (lastAsp / (lastAsp + curAsp));

			if (lastHigh.x > 0.0 && current.x > 0.0)
				correctionHigh.x = prop * lastHigh.x + (1.0 - prop) * current.x;
			if (lastHigh.y > 0.0 && current.y > 0.0)
				correctionHigh.y = prop * lastHigh.y + (1.0 - prop) * current.y;
			correctionHigh.z += prop * lastHigh.z + (1.0 - prop) * current.z;
			break;
		}

	double3 correctionMid{};
	auto iterMid = m.mid.begin();
	auto lastMid = *(iterMid++);
	for (; iterMid != m.mid.end(); lastMid = *(iterMid++))
		if ((lastMid.w >= a.z && a.z >= (*iterMid).w))
		{
			auto& current = (*iterMid);
			double lastAsp = lastMid.w - a.z;
			double curAsp = a.z - current.w;
			double prop = 1 - (lastAsp / (lastAsp + curAsp));

			if (lastMid.x > 0 && current.x > 0)
				correctionMid.x = prop * lastMid.x + (1 - prop) * current.x;
			if (lastMid.y > 0 && current.y > 0)
				correctionMid.y = prop * lastMid.y + (1 - prop) * current.y;
			correctionMid.z += prop * lastMid.z + (1 - prop) * current.z;
			break;
		}

	double3 correctionLow{};
	auto iterLow = m.low.begin();
	auto lastLow = *(iterLow++);
	for (; iterLow != m.low.end(); lastLow = *(iterLow++))
		if ((lastLow.w >= a.z && a.z >= (*iterLow).w))
		{
			auto& current = (*iterLow);
			double lastAsp = lastLow.w - a.z;
			double curAsp = a.z - current.w;
			double prop = 1 - (lastAsp / (lastAsp + curAsp));

			if (lastLow.x > 0 && current.x > 0)
				correctionLow.x = prop * lastLow.x + (1 - prop) * current.x;
			if (lastLow.y > 0 && current.y > 0)
				correctionLow.y = prop * lastLow.y + (1 - prop) * current.y;
			correctionLow.z += prop * lastLow.z + (1 - prop) * current.z;
			break;
		}

	double3 aHigh = a;
	if (correctionHigh.x > 0)
		aHigh.x *= correctionHigh.x;
	if (correctionHigh.y > 0)
		aHigh.y *= correctionHigh.y;
	aHigh.z += correctionHigh.z;

	double3 pHigh = from_XYZ_to_sRGB(lch_to_xyz(aHigh) / 100.0);


	double3 aLow = a;
	if (correctionLow.x > 0)
		aLow.x *= correctionLow.x;
	if (correctionLow.y > 0)
		aLow.y *= correctionLow.y;
	aLow.z += correctionLow.z;

	double3 pLow = from_XYZ_to_sRGB(lch_to_xyz(aLow) / 100.0);
	double max = std::max(linalg::maxelem(pLow), linalg::maxelem(pHigh));

	if (128.0 / 255.0 >= max)
	{
		if (correctionLow.x > 0)
		{
			a.x *= correctionLow.x;
		}
		if (correctionLow.y > 0)
		{
			a.y *= correctionLow.y;
		}
		a.z += correctionLow.z;
	}
	else if (192.0 / 255.0 >= max)
	{
		double lenLow = std::abs((128.0 / 255.0) - max);
		double lenMid = std::abs((192.0 / 255.0) - max);
		double aspectMid = (1.0 - lenMid / (lenLow + lenMid));

		if (correctionLow.x > 0 && correctionMid.x > 0)
		{
			a.x *= correctionMid.x * aspectMid + correctionLow.x * (1.0 - aspectMid);
		}
		if (correctionLow.y > 0 && correctionMid.y > 0)
		{
			a.y *= correctionMid.y * aspectMid + correctionLow.y * (1.0 - aspectMid);
		}
		a.z += correctionMid.z * aspectMid + correctionLow.z * (1.0 - aspectMid);
	}
	else
	{
		double lenMid = std::abs((192.0 / 255.0) - max);
		double lenHigh = std::abs(1.0 - max);
		double aspectHigh = (1.0 - lenHigh / (lenMid + lenHigh));

		if (correctionMid.x > 0 && correctionHigh.x > 0)
		{
			a.x *= correctionHigh.x * aspectHigh + correctionMid.x * (1.0 - aspectHigh);
		}
		if (correctionMid.y > 0 && correctionHigh.y > 0)
		{
			a.y *= correctionHigh.y * aspectHigh + correctionMid.y * (1.0 - aspectHigh);
		}
		a.z += correctionHigh.z * aspectHigh + correctionMid.z * (1.0 - aspectHigh);
	}


	p = from_XYZ_to_sRGB(lch_to_xyz(a) / 100.0);

}

void LutCalibrator::printReport()
{
	QStringList info;

	info.append("-------------------------------------------------------------------------------------------------");
	info.append("                                        Detailed results");
	info.append("-------------------------------------------------------------------------------------------------");
	for (int r = 0; r < SCREEN_COLOR_DIMENSION; r++)
		for (int g = 0; g < SCREEN_COLOR_DIMENSION; g++)
			for (int b = 0; b < SCREEN_COLOR_DIMENSION; b++)
				if ((r % 4 == 0 && g % 4 == 0 && b % 4 == 0) || _debug)
				{
					const auto& sample = _capturedColors->all[r][g][b];
					auto list = sample.getFinalRGB();

					QStringList colors;						
					for (auto i = list.begin(); i != list.end(); i++)
					{
						colors.append(QString("%1").arg(vecToString(*i), 12));
					}
					info.append(QString("%1 => %2 %3")
						.arg(vecToString(sample.getSourceRGB()), 12)
						.arg(colors.join(", "))
						.arg(((list.size() > 1)?"[source noise detected]" : "")));
				}

	info.append("-------------------------------------------------------------------------------------------------");
	sendReport(_log, info.join("\r\n"));
}


static double3 hdr_to_srgb(const YuvConverter* _yuvConverter, double3 yuv, const linalg::vec<uint8_t, 2>& UV, const double3& aspect, const double4x4& coefMatrix, ColorSpaceMath::HDR_GAMMA gamma, double gammaHLG, double nits, int altConvert, const double3x3& bt2020_to_sRgb, int tryBt2020Range, const BestResult::Signal& signal, int colorAspectMode, const std::pair<double3, double3>& colorAspect)
{	
	double3 srgb;
	bool white = true;

	if (gamma == HDR_GAMMA::P010)
	{
		unpackP010(yuv);
	}

	if (gamma == HDR_GAMMA::sRGB || gamma == HDR_GAMMA::BT2020inSRGB)
	{
		CapturedColors::correctYRange(yuv, signal.yRange, signal.upYLimit, signal.downYLimit, signal.yShift);
	}

	if (signal.range == YuvConverter::COLOR_RANGE::LIMITED)
	{
		const double low = 16.0 / 255.0;
		yuv.x = (yuv.x < low) ? low : (yuv.x - low) * aspect.x + low;
	}
	else
	{
		yuv.x *= aspect.x;
	}
	if (UV.x != UV.y || UV.x != 128)
	{
		const double mid = 128.0 / 256.0;
		yuv.y = (yuv.y - mid) * aspect.y + mid;
		yuv.z = (yuv.z - mid) * aspect.z + mid;
		white = false;
	}

	auto a = _yuvConverter->multiplyColorMatrix(coefMatrix, yuv);

	double3 e;

	if (gamma == HDR_GAMMA::PQ || gamma == HDR_GAMMA::P010)
	{
		e = PQ_ST2084(10000.0 / nits, a);
	}
	else if (gamma == HDR_GAMMA::HLG)
	{
		e = OOTF_HLG(inverse_OETF_HLG(a), gammaHLG) * nits;
	}
	else if (gamma == HDR_GAMMA::sRGB)
	{
		srgb = a;
	}
	else if (gamma == HDR_GAMMA::BT2020inSRGB)
	{
		e = srgb_nonlinear_to_linear(a);
	}
	else if (gamma == HDR_GAMMA::PQinSRGB)
	{
		a = srgb_linear_to_nonlinear(a);
		e = PQ_ST2084(10000.0 / nits, a);
	}


	if (gamma != HDR_GAMMA::sRGB)
	{

		if (altConvert)
		{
			srgb = mul(bt2020_to_sRgb, e);
		}
		else
		{
			srgb = ColorSpaceMath::from_BT2020_to_BT709(e);
		}		
	
		srgb = srgb_linear_to_nonlinear(srgb);
	}	

	if (tryBt2020Range)
	{
		srgb = bt2020_nonlinear_to_linear(srgb);
		srgb = srgb_linear_to_nonlinear(srgb);
	}

	if (colorAspectMode)
	{
		if (colorAspectMode == 1)
		{
			if (!white)
			{
				srgb *= (colorAspect).first;
			}
			else
			{
				double av = ((colorAspect).first.x + (colorAspect).first.y + (colorAspect).first.z) / 3.0;
				srgb *= av;
			}
		}
		else if (colorAspectMode == 2)
		{
			if (!white)
			{
				srgb *= ((colorAspect).first + (colorAspect).second) * 0.5;
			}
			else
			{
				double av = ((colorAspect).first.x + (colorAspect).first.y + (colorAspect).first.z + (colorAspect).second.x + (colorAspect).second.y + (colorAspect).second.z) / 6.0;
				srgb *= av;
			}
		}
		else
		{
			if (!white)
			{
				srgb *= (colorAspect).second;
			}
			else
			{
				double av = ((colorAspect).second.x + (colorAspect).second.y + (colorAspect).second.z) / 3.0;
				srgb *= av;
			}
		}

	}

	ColorSpaceMath::clamp01(srgb);

	return srgb;
}

static LchLists prepareLCH(std::list<std::list<std::pair<double3, double3>>> __lchPrimaries)
{
	int index = 0;
	LchLists ret;
	
	for (const auto& _lchPrimaries : __lchPrimaries)
	{
		std::list<double4> lchPrimaries;
		for (const auto& c : _lchPrimaries)
		{
			const auto& org = c.first;
			auto current = xyz_to_lch(from_sRGB_to_XYZ(c.second) * 100.0);

			double4 correctionZ;
			correctionZ.x = (current.x != 0.0) ? org.x / current.x : 0.0;
			correctionZ.y = (current.y != 0.0) ? org.y / current.y : 0.0;
			correctionZ.z = org.z - current.z;
			correctionZ.w = current.z;
			lchPrimaries.push_back(correctionZ);
		}

		lchPrimaries.sort([](const double4& a, const double4& b) { return a.w > b.w; });

		double4 loopEnd = lchPrimaries.front();
		double4 loopFront = lchPrimaries.back();

		loopEnd.w -= 360.0;
		lchPrimaries.push_back(loopEnd);

		loopFront.w += 360.0;
		lchPrimaries.push_front(loopFront);

		if (index == 0)
			ret.low = lchPrimaries;
		else if (index == 1)
			ret.mid = lchPrimaries;
		else if (index == 2)
			ret.high = lchPrimaries;
		index++;
	}

	return ret;
}

void CalibrationWorker::run()
{
	constexpr int MAX_HINT = std::numeric_limits<int>::max() / 2.0;
	constexpr double MAX_HDOUBLE = std::numeric_limits<double>::max() / 2.0;

	printf("Starter thread: %i. Range: [%i - %i)\n", id, krIndexStart, krIndexEnd);

	for (int krIndex = krIndexStart; krIndex < std::min(krIndexEnd, (halfKDelta * 2) + 1); krIndex++)
	for (int kbIndex = 0; kbIndex <= 2 * halfKDelta; kbIndex ++)
	{
		if (!precise)
		{
			QString gammaString = ColorSpaceMath::gammaToString(gamma);
			QString coefString = yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef));
			if (gamma == ColorSpaceMath::HDR_GAMMA::HLG)
				gammaString += QString(" %1").arg(gammaHLG);
			emit notifyCalibrationMessage(QString("First phase progress<br/>Gamma: %1<br/>Coef: %2<br/>%3/289").arg(gammaString).arg(coefString).arg(++progress), false);
		}
		else
		{
			emit notifyCalibrationMessage(QString("Second phase progress:<br/>%1/1089").arg(++progress), false);
		}

		double2 kDelta = double2(((krIndex <= halfKDelta) ? -krIndex : krIndex - halfKDelta),
				((kbIndex <= halfKDelta) ? -kbIndex : kbIndex - halfKDelta)) * ((precise) ? 0.002 : 0.004);

		auto coefValues = yuvConverter->getCoef(YuvConverter::YUV_COEFS(coef)) + kDelta;
		auto coefMatrix = yuvConverter->create_yuv_to_rgb_matrix(bestResult.signal.range, coefValues.x, coefValues.y);
		std::list<int> coloredAspectModeList;

		if (!precise)
		{
			if (bestResult.signal.isSourceP010)
			{
				coloredAspectModeList = { 0 };
			}
			else
			{
				coloredAspectModeList = { 0, 1, 2, 3 };
			}
		}
		else if (bestResult.coloredAspectMode != 0)
			coloredAspectModeList = { 0,  bestResult.coloredAspectMode };
		else
			coloredAspectModeList = { 0};

		for (const int& coloredAspectMode : coloredAspectModeList)
			for (int altConvert = (precise) ? bestResult.altConvert : 0; altConvert <= 1; altConvert += (precise) ? MAX_HINT : 1)
				for (int tryBt2020Range = (precise) ? bestResult.bt2020Range : 0; tryBt2020Range <= 1; tryBt2020Range += (precise) ? MAX_HINT : 1)
					for (double aspectX = 0.99; aspectX <= 1.0101; aspectX += ((precise) ? 0.0025 : 0.0025 * 2.0))
					{
						for (double aspectYZ = 1.0; aspectYZ <= 1.2101; aspectYZ += ((precise) ? MAX_HDOUBLE : 0.005 * 2.0))
							for (double aspectY = bestResult.aspect.y - 0.02; aspectY <= bestResult.aspect.y + 0.021; aspectY += (precise) ? 0.005 : MAX_HDOUBLE)
								for (double aspectZ = bestResult.aspect.z - 0.02; aspectZ <= bestResult.aspect.z + 0.021; aspectZ += (precise) ? 0.005 : MAX_HDOUBLE)
								{
									double3 aspect = (precise) ? double3{ aspectX, aspectY, aspectZ } : double3{ aspectX, aspectYZ, aspectYZ };

									std::pair<double3, double3> colorAspect = std::pair<double3, double3>({ 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 });

									std::list<std::pair<double3,double3>> selectedLchHighPrimaries, selectedLchMidPrimaries, selectedLchLowPrimaries;

									if (coloredAspectMode)
									{
										auto red = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::RED].first, sampleColors[SampleColor::RED].second, aspect, coefMatrix, gamma, gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										auto green = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::GREEN].first, sampleColors[SampleColor::GREEN].second, aspect, coefMatrix, gamma, gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										auto blue = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::BLUE].first, sampleColors[SampleColor::BLUE].second, aspect, coefMatrix, gamma, gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										colorAspect.first = double3{ 1.0 / red.x, 1.0 / green.y, 1.0 / blue.z };

										red = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::LOW_RED].first, sampleColors[SampleColor::LOW_RED].second, aspect, coefMatrix, gamma, gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										green = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::LOW_GREEN].first, sampleColors[SampleColor::LOW_GREEN].second, aspect, coefMatrix, gamma, gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										blue = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::LOW_BLUE].first, sampleColors[SampleColor::LOW_BLUE].second, aspect, coefMatrix, gamma, gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										colorAspect.second = double3{ 0.5 / red.x, 0.5 / green.y, 0.5 / blue.z };
									}

									long long int currentError = 0;

									for (auto v = vertex.begin(); v != vertex.end(); ++v)
									{
										auto& sample = *(*v).first;

										auto minError = MAX_CALIBRATION_ERROR;

										if (sample.U() == 128 && sample.V() == 128)
										{
											(*v).second = hdr_to_srgb(yuvConverter, sample.yuv(), byte2{ sample.U(), sample.V() }, aspect, coefMatrix, gamma, gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, coloredAspectMode, colorAspect);
											auto SRGB = to_int3((*v).second * 255.0);
											minError = sample.getSourceError(SRGB);
										}
										else
										{											
											auto sampleList = sample.getInputYuvColors();
											for (auto iter = sampleList.cbegin(); iter != sampleList.cend(); ++iter)
											{
												auto srgb = hdr_to_srgb(yuvConverter, (*iter).first, byte2{ sample.U(), sample.V() }, aspect, coefMatrix, gamma, gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, coloredAspectMode, colorAspect);

												auto SRGB = to_int3(srgb * 255.0);

												auto sampleError = sample.getSourceError(SRGB);

												if (sampleError < minError)
												{
													(*v).second = srgb;
													minError = sampleError;
												}
											}
										}

										currentError += minError;

										if (((!precise || !lchCorrection) && (currentError >= bestResult.minError || currentError > weakBestScore))
											|| currentError >= MAX_CALIBRATION_ERROR)
										{
											currentError = MAX_CALIBRATION_ERROR;
											break;
										}

										if (precise)
										{
											double3 lchPrimaries;
											auto res = (*v).first->isLchPrimary(&lchPrimaries);
											if (res == CapturedColor::LchPrimaries::HIGH)
											{
												selectedLchHighPrimaries.push_back(std::pair<double3, double3>(lchPrimaries, (*v).second));
											}
											else if (res == CapturedColor::LchPrimaries::MID)
											{
												selectedLchMidPrimaries.push_back(std::pair<double3, double3>(lchPrimaries, (*v).second));
											}
											else if (res == CapturedColor::LchPrimaries::LOW)
											{
												selectedLchLowPrimaries.push_back(std::pair<double3, double3>(lchPrimaries, (*v).second));
											}
										}
										
									}									

									bool lchFavour = false;
									long long int lcHError = MAX_CALIBRATION_ERROR;

									LchLists selectedLchPrimaries;
									if (precise && lchCorrection && currentError < MAX_CALIBRATION_ERROR)
									{
										lcHError = 0;
										lchFavour = true;

										selectedLchPrimaries = prepareLCH({ selectedLchLowPrimaries, selectedLchMidPrimaries, selectedLchHighPrimaries  });
										
										for (auto  sample = vertex.begin();  sample != vertex.end(); ++sample)
										{
											auto correctedRGB = (*sample).second;

											doToneMapping(selectedLchPrimaries, correctedRGB);
											lcHError += (*sample).first->getSourceError(round_to_0_255<int3>(correctedRGB * 255.0));
											if (lcHError >= currentError || lcHError > weakBestScore)
											{
												lchFavour = false;
												lcHError = MAX_CALIBRATION_ERROR;
												break;
											}
										}										
									}
									else
										lcHError = MAX_CALIBRATION_ERROR;
									

									if (currentError < bestResult.minError || lcHError < bestResult.minError)
									{										
										bestResult.minError = (lchFavour) ? lcHError  : currentError;

										if (weakBestScore > bestResult.minError)
											weakBestScore = bestResult.minError;

										bestResult.coef = YuvConverter::YUV_COEFS(coef);
										bestResult.coefDelta = kDelta;
										bestResult.bt2020Range = tryBt2020Range;
										bestResult.altConvert = altConvert;
										bestResult.altPrimariesToSrgb = bt2020_to_sRgb;
										bestResult.coloredAspectMode = coloredAspectMode;
										bestResult.colorAspect = colorAspect;
										bestResult.aspect = aspect;
										bestResult.nits = NITS;
										bestResult.gamma = gamma;
										bestResult.gammaHLG = gammaHLG;
										bestResult.coefMatrix = coefMatrix;
										bestResult.lchEnabled = (lchFavour);
										bestResult.lchPrimaries = selectedLchPrimaries;
										printf("New local best score: %.3f (classic: %.3f, LCH: %.3f %s) for thread: %i. Gamma: %s, coef: %s, kr/kb: %s, yuvCorrection: %s\n", 
											bestResult.minError / 300.0,
											currentError / 300.0,
											lcHError / 300.0,
											(bestResult.lchEnabled) ? "ON" : "OFF",
											id,
											QSTRING_CSTR(gammaToString(gamma)),
											QSTRING_CSTR(yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef))),
											QSTRING_CSTR(vecToString(kDelta)),
											QSTRING_CSTR(vecToString(aspect)));
									}
								}
						if (forcedExit)
						{
							printf("User terminated thread: %i\n", id);
							return;
						}
					}
	}

	if (bestResult.minError < MAX_CALIBRATION_ERROR)
		printf("Finished thread: %i. Score: %.3f\n", id, bestResult.minError / 300.0);		
	else
		printf("Finished thread: %i. Could not find anything\n", id);
		
}

void  LutCalibrator::fineTune(bool precise)
{
	constexpr auto MAX_IND = SCREEN_COLOR_DIMENSION - 1;
	const auto white = _capturedColors->all[MAX_IND][MAX_IND][MAX_IND].Y();
	double NITS = 0.0;
	double maxLevel = 0.0;
	std::atomic<int> weakBestScore = MAX_CALIBRATION_ERROR;

	// prepare vertexes
	std::list<CapturedColor*> vertex;

	for (int r = MAX_IND; r >= 0; r--)
		for (int g = MAX_IND; g >= 0; g--)
			for (int b = MAX_IND; b >= 0; b--)
			{
				
				if ((r % 4 == 0 && g % 4 == 0 && b % 2 == 0) || (r == g * 2 && g > b) || (r <= 6 && g <= 6 && b <= 6) || (r == b && b == g) || (r == g && r > 0) || (r == b && r > 0)
					|| _capturedColors->all[r][g][b].isLchPrimary(nullptr) != CapturedColor::LchPrimaries::NONE
					|| (bestResult->signal.isSourceP010 && ((r - g > 0 && r - g <= 3 && b == 0) || (r > 0 && g == 0 && b == 0) || (r == 0 && g > 0 && b == 0) || (r == 0 && g == 0 && b > 0))))
				{

					vertex.push_back(&_capturedColors->all[r][g][b]);
				}
			}

	vertex.sort([](CapturedColor*& a, CapturedColor*& b) { return ((int)a->coords().x + a->coords().y + a->coords().z) >
																				((int)b->coords().x + b->coords().y + b->coords().z); });

	if (!precise)
	{
		Info(_log, "The first phase starts");
		Info(_log, "Optimal thread count: %i", QThreadPool::globalInstance()->maxThreadCount());
		Info(_log, "Number of test vertexes: %i", vertex.size());
	}
	else
	{
		Info(_log, "The second phase starts");
	}

	// set startup parameters (signal)
	bestResult->signal.range = _capturedColors->getRange();
	_capturedColors->getSignalParams(bestResult->signal.yRange, bestResult->signal.upYLimit, bestResult->signal.downYLimit, bestResult->signal.yShift, bestResult->signal.yuvRange);

	if (bestResult->signal.isSourceP010)
	{
		double up = bestResult->signal.upYLimit;
		unpackP010(&up, nullptr, nullptr);
		bestResult->signal.upYLimit = up;

		double down = bestResult->signal.downYLimit;
		unpackP010(&down, nullptr, nullptr);
		bestResult->signal.downYLimit = down;

		double3 yuvrange = static_cast<double3>(bestResult->signal.yuvRange);
		yuvrange.x /= 255.0;
		yuvrange.y = (yuvrange.y - 128.0) / 128.0;
		yuvrange.z = (yuvrange.z - 128.0) / 128.0;
		unpackP010(yuvrange);
		yuvrange.x *= 255.0;
		yuvrange.y = yuvrange.y * 128.0 + 128.0;
		yuvrange.z = yuvrange.z * 128.0 + 128.0;
		bestResult->signal.yuvRange = static_cast<byte3>(yuvrange);
	}

	if (bestResult->signal.range == YuvConverter::COLOR_RANGE::LIMITED)
	{
		maxLevel = (white - 16.0) / (235.0 - 16.0);
	}
	else
	{
		maxLevel = white / 255.0;
	}	

	std::vector<std::pair<double3, byte2>> sampleColors(6);
	const auto& sampleRed = _capturedColors->all[MAX_IND][0][0];
	const auto& sampleGreen = _capturedColors->all[0][MAX_IND][0];
	const auto& sampleBlue = _capturedColors->all[0][0][MAX_IND];
	const auto& sampleRedLow = _capturedColors->all[MAX_IND / 2][0][0];
	const auto& sampleGreenLow = _capturedColors->all[0][MAX_IND / 2][0];
	const auto& sampleBlueLow = _capturedColors->all[0][0][MAX_IND / 2];

	sampleColors[SampleColor::RED] = (std::pair<double3, byte2>(sampleRed.getInputYuvColors().front().first, byte2{ sampleRed.U(), sampleRed.V() }));
	sampleColors[SampleColor::GREEN] = (std::pair<double3, byte2>(sampleGreen.getInputYuvColors().front().first, byte2{ sampleGreen.U(), sampleGreen.V() }));
	sampleColors[SampleColor::BLUE] = (std::pair<double3, byte2>(sampleBlue.getInputYuvColors().front().first, byte2{ sampleBlue.U(), sampleBlue.V() }));

	sampleColors[SampleColor::LOW_RED] = (std::pair<double3, byte2>(sampleRedLow.getInputYuvColors().front().first, byte2{ sampleRedLow.U(), sampleRedLow.V() }));
	sampleColors[SampleColor::LOW_GREEN] = (std::pair<double3, byte2>(sampleGreenLow.getInputYuvColors().front().first, byte2{ sampleGreenLow.U(), sampleGreenLow.V() }));
	sampleColors[SampleColor::LOW_BLUE] = (std::pair<double3, byte2>(sampleBlueLow.getInputYuvColors().front().first, byte2{ sampleBlueLow.U(), sampleBlueLow.V() }));

	for (int gammaInt = (precise ? static_cast<int>(bestResult->gamma)
		: (bestResult->signal.isSourceP010
			? static_cast<int>(ColorSpaceMath::HDR_GAMMA::P010)
			: static_cast<int>(ColorSpaceMath::HDR_GAMMA::PQ)));
		gammaInt <= static_cast<int>(ColorSpaceMath::HDR_GAMMA::P010);
		gammaInt++)
	{
		auto gamma = static_cast<ColorSpaceMath::HDR_GAMMA>(gammaInt);
		std::vector<double> gammasHLG;

		if (gamma == HDR_GAMMA::P010 && !bestResult->signal.isSourceP010)
			continue;
			
		if (gamma == HDR_GAMMA::HLG)
		{
			if (precise)
			{
				gammasHLG = { bestResult->gammaHLG };
			}
			else
			{
				gammasHLG = { 0 , 1.2, 1.1 };
			}
		}
		else
			gammasHLG = { 0 };

		if (gamma == HDR_GAMMA::PQ)
		{
			NITS = 10000.0 * PQ_ST2084(1.0, maxLevel);
		}
		else if (gamma == HDR_GAMMA::P010)
		{
			double unpackWhite = white / 255.0;
			unpackP010(&unpackWhite, nullptr, nullptr);
			unpackWhite *= 255.0;
			if (bestResult->signal.range == YuvConverter::COLOR_RANGE::LIMITED)
			{
				maxLevel = (unpackWhite - 16.0) / (235.0 - 16.0);
			}
			else
			{
				maxLevel = unpackWhite / 255.0;
			}
			NITS = 10000.0 * PQ_ST2084(1.0, maxLevel);
		}
		else if (gamma == HDR_GAMMA::PQinSRGB)
		{
			NITS = 10000.0 * PQ_ST2084(1.0, srgb_linear_to_nonlinear(maxLevel));
		}

		for (double gammaHLG : gammasHLG)
		{
			if (gammaHLG == 1.1 && bestResult->gamma != HDR_GAMMA::HLG)
				break;

			if (gamma == HDR_GAMMA::HLG)
			{
				NITS = 1.0 / OOTF_HLG(inverse_OETF_HLG(maxLevel), gammaHLG).x;
			}
			for (int coef = (precise) ? bestResult->coef : YuvConverter::YUV_COEFS::BT601; coef <= YuvConverter::YUV_COEFS::BT2020; coef++)
			{
				double3x3 convert_bt2020_to_XYZ;
				double3x3 convert_XYZ_to_sRgb;

				capturedPrimariesCorrection(gamma, gammaHLG, NITS, coef, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb);
				auto bt2020_to_sRgb = mul(convert_XYZ_to_sRgb, convert_bt2020_to_XYZ);

				printf("Processing gamma: %s, gammaHLG: %f, coef: %s. Current best gamma: %s, gammaHLG: %f, coef: %s (d:%s). Score: %.3f\n",
					QSTRING_CSTR(gammaToString(gamma)), gammaHLG, QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef))),
					QSTRING_CSTR(gammaToString(bestResult->gamma)), bestResult->gammaHLG, QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(bestResult->coef))),
					QSTRING_CSTR(vecToString(bestResult->coefDelta)),bestResult->minError / 300.0);

				const int halfKDelta = (precise) ? 16 : 8;
				const int krDelta = std::ceil((halfKDelta * 2.0) / QThreadPool::globalInstance()->maxThreadCount());

				QList<CalibrationWorker*> workers;
				int index = 0;
				std::atomic<int> progress = 0;

				for (int krIndexStart = 0; krIndexStart <= halfKDelta * 2; krIndexStart += krDelta)
				{
					auto worker = new CalibrationWorker(bestResult.get(), weakBestScore, _yuvConverter.get(), index++, krIndexStart, krIndexStart + krDelta, halfKDelta, precise, coef, sampleColors, gamma, gammaHLG, NITS, bt2020_to_sRgb, vertex, _lchCorrection, _forcedExit, progress);
					workers.push_back(worker);
					QThreadPool::globalInstance()->start(worker);
					connect(worker, &CalibrationWorker::notifyCalibrationMessage, this, &LutCalibrator::notifyCalibrationMessage, Qt::DirectConnection);
				}
				QThreadPool::globalInstance()->waitForDone();

				for (const auto& worker : workers)
				{
					worker->getBestResult(bestResult.get());
				}

				qDeleteAll(workers);
				workers.clear();

				if (precise || _forcedExit)
					break;
			}			
		}

		if (precise || _forcedExit)
			break;
	}
}

static void reportLCH(Logger* _log, std::vector<std::vector<std::vector<CapturedColor>>>* all);


void LutCalibrator::calibration()
{
	// calibration
	auto totalTime = InternalClock::now();

	fineTune(false);

	if (_forcedExit)
	{
		error("User terminated calibration");
		return;
	}

	totalTime = InternalClock::now() - totalTime;

	if (bestResult->minError >= MAX_CALIBRATION_ERROR)
	{
		error("The calibration failed. The error is too high.");
		return;
	}

	// fine tuning
	auto totalTime2 = InternalClock::now();

	fineTune(true);

	if (_forcedExit)
	{
		error("User terminated calibration");
		return;
	}

	totalTime2 = InternalClock::now() - totalTime2;
	
	// write result
	Debug(_log, "Score: %.3f", bestResult->minError / 300.0);
	Debug(_log, "LCH: %s", (bestResult->lchEnabled) ? "Enabled" : "Disabled");
	Debug(_log, "The first phase time: %.3fs", totalTime / 1000.0);
	Debug(_log, "The second phase time: %.3fs", totalTime2 / 1000.0);
	Debug(_log, "Selected coef: %s", QSTRING_CSTR( _yuvConverter->coefToString(bestResult->coef)));
	Debug(_log, "Selected coef delta: %f %f", bestResult->coefDelta.x, bestResult->coefDelta.y);
	Debug(_log, "Selected EOTF: %s", QSTRING_CSTR(ColorSpaceMath::gammaToString(bestResult->gamma)));
	if (bestResult->gamma == HDR_GAMMA::HLG)
	{
		Debug(_log, "Selected HLG gamma: %f", bestResult->gammaHLG);
	}
	if (bestResult->gamma != HDR_GAMMA::sRGB && bestResult->gamma != HDR_GAMMA::BT2020inSRGB)
	{
		Debug(_log, "Selected nits: %f", (bestResult->gamma == HDR_GAMMA::HLG) ? 1000.0 * (1 / bestResult->nits) : bestResult->nits);
	}
	Debug(_log, "Selected bt2020 gamma range: %s", (bestResult->bt2020Range) ? "yes" : "no");
	Debug(_log, "Selected alternative conversion of primaries: %s", (bestResult->altConvert) ? "yes" : "no");
	Debug(_log, "Selected aspect: %f %f %f", bestResult->aspect.x, bestResult->aspect.y, bestResult->aspect.z);
	Debug(_log, "Selected color aspect mode: %i", bestResult->coloredAspectMode);
	Debug(_log, "Selected color aspect: %s %s", QSTRING_CSTR(vecToString(bestResult->colorAspect.first)), QSTRING_CSTR(vecToString(bestResult->colorAspect.second)));
	Debug(_log, "Selected source is P010: %s", (bestResult->signal.isSourceP010) ? "yes" : "no");

	if (_debug)
	{
		double3x3 convert_bt2020_to_XYZ;
		double3x3 convert_XYZ_to_sRgb;

		capturedPrimariesCorrection(bestResult->gamma, bestResult->gammaHLG, bestResult->nits, bestResult->coef, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb, true);
	}

	// write report (captured raw colors)
	std::stringstream results;
	bestResult->serialize(results);
	QString fileLogName = QString("%1%2").arg(_rootPath).arg("/calibration_captured_yuv.txt");
	if (_capturedColors->saveResult(QSTRING_CSTR(fileLogName), results.str()))
	{
		Info(_log, "Write captured colors to: %s", QSTRING_CSTR(fileLogName));
	}
	else
	{
		Error(_log, "Could not save captured colors to: %s", QSTRING_CSTR(fileLogName));
	}

	// create LUT
	notifyCalibrationMessage("Writing final LUT...");

	_lut.releaseMemory();

	auto totalTime3 = InternalClock::now();
	QString errorMessage = CreateLutFile(_log, _rootPath, bestResult.get(), &(_capturedColors->all));
	totalTime3 = InternalClock::now() - totalTime3;

	if (!errorMessage.isEmpty())
	{
		error(errorMessage);
	}
	else
	{
		Debug(_log, "The LUT creation time: %.3fs", totalTime3 / 1000.0);
	}

	// LCH
	reportLCH(_log, &(_capturedColors->all));

	// control score
	long long int currentError = 0;
	constexpr auto MAX_IND = SCREEN_COLOR_DIMENSION - 1;
	for (int r = MAX_IND; r >= 0; r--)
		for (int g = MAX_IND; g >= 0; g--)
			for (int b = MAX_IND; b >= 0; b--)
			{
				if ((r % 4 == 0 && g % 4 == 0 && b % 2 == 0) || (r == g * 2 && g > b) || (r <= 6 && g <= 6 && b <= 6) || (r == b && b == g) || (r == g && r > 0) || (r == b && r > 0)
					|| _capturedColors->all[r][g][b].isLchPrimary(nullptr) != CapturedColor::LchPrimaries::NONE
					|| (bestResult->signal.isSourceP010 && ((r - g > 0 && r - g <= 3 && b == 0) || (r > 0 && g == 0 && b == 0) || (r == 0 && g > 0 && b == 0) || (r == 0 && g == 0 && b > 0))))
				{
					auto sample = _capturedColors->all[r][g][b];
					auto sampleList = sample.getFinalRGB();
					long long int microError = MAX_CALIBRATION_ERROR;
					for (auto iter = sampleList.cbegin(); iter != sampleList.cend(); ++iter)
					{
						auto c = sample.getSourceError((int3)*iter);
						if (c < microError)
							microError = c;
					}
					currentError += microError;
				}
			}
	Debug(_log, "The control score: %.3f", currentError / 300.0);

	// reload LUT
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, false);
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);

	if (_defaultComp == hyperhdr::COMP_VIDEOGRABBER)
	{
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_VIDEOGRABBER, -1, true);	
	}
	if (_defaultComp == hyperhdr::COMP_FLATBUFSERVER)
	{
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_FLATBUFSERVER, -1, true);
	}
}


static void reportLCH(Logger* _log, std::vector<std::vector<std::vector<CapturedColor>>>* all)
{
	QStringList info, intro;
	std::list<MappingPrime> mHigh;
	std::list<MappingPrime> mMid;
	std::list<MappingPrime> mLow;

	
	constexpr auto MAX_IND = SCREEN_COLOR_DIMENSION - 1;
	for (int r = MAX_IND; r >= 0; r--)
		for (int g = MAX_IND; g >= 0; g--)
			for (int b = MAX_IND; b >= 0; b--)
			{
				double3 org;
				auto ret = (*all)[r][g][b].isLchPrimary(&org);
				if (ret == CapturedColor::LchPrimaries::HIGH || ret == CapturedColor::LchPrimaries::MID || ret == CapturedColor::LchPrimaries::LOW)
				{
					MappingPrime m;
					m.org = org;
					m.prime = byte3(r, g, b);
					if (ret == CapturedColor::LchPrimaries::HIGH)
						mHigh.push_back(m);
					else if (ret == CapturedColor::LchPrimaries::MID)
						mMid.push_back(m);
					else
						mLow.push_back(m);
				}
			}

	
	for (std::list<MappingPrime>*& m : std::list<std::list<MappingPrime>*>{ &mHigh, &mMid, &mLow })
	{
		for (MappingPrime& c : *m)
		{
			auto& sample = (*all)[c.prime.x][c.prime.y][c.prime.z];
			auto b = static_cast<double3>(sample.getFinalRGB().front()) / 255.0;
			c.real = xyz_to_lch(from_sRGB_to_XYZ(b) * 100.0);
			c.delta = c.org - c.real;
		}
		m->sort([](const MappingPrime& a, const MappingPrime& b) { return a.real.z > b.real.z; });
		MappingPrime loopEnd = m->front();
		MappingPrime loopFront = m->back();

		loopEnd.org.z -= 360;
		loopEnd.real.z -= 360;
		m->push_back(loopEnd);

		loopFront.org.z += 360;
		loopFront.real.z += 360;
		m->push_front(loopFront);
	}
	
	info.append("Primaries in LCH colorspace");
	info.append("RGB           | RGB primary in LCH        | captured primary in LCH   |   average LCH delta       |  LCH to RGB way back ");
	info.append("--------------------------------------------------------------------------------------------------------------------------------------------------------");
	for (std::list<MappingPrime>*& m : std::list<std::list<MappingPrime>*>{ &mHigh, &mMid, &mLow })
	{
		for (MappingPrime& c : *m)
		{
			auto& sample = (*all)[c.prime.x][c.prime.y][c.prime.z];
			auto aa = from_XYZ_to_sRGB(lch_to_xyz(c.org) / 100.0) * 255;
			auto bb = from_XYZ_to_sRGB(lch_to_xyz(c.real) / 100.0) * 255;
			info.append(QString("%1 | %2 | %3 | %4 | %5 %6").arg(vecToString(sample.getSourceRGB()), 12).
				arg(vecToString(c.org)).
				arg(vecToString(c.real)).				
				arg(vecToString(c.delta)).
				arg(vecToString(round_to_0_255<byte3>(aa))).
				arg(vecToString(round_to_0_255<byte3>(bb))));

		}
		info.append("--------------------------------------------------------------------------------------------------------------------------------------------------------");
	}

	LutCalibrator::sendReport(_log, info.join("\r\n"));
}

void CreateLutWorker::run()
{
	printf("Starting LUT creation thread for phase %i. V range is [ %i, %i)\n", phase, startV, endV);
	for (int v = startV; v < endV; v++)
		for (int u = 0; u <= 255; u++)
			for (int y = 0; y <= 255; y++)
			{
				byte3 YUV(y, u, v);
				double3 yuv = to_double3(YUV) / 255.0;

				if (phase == 0)
				{
					yuv = yuvConverter->toYuv(bestResult->signal.range, bestResult->coef, yuv);
					YUV = round_to_0_255<byte3>(yuv * 255);
				}

				if (phase == 0 || phase == 1)
				{
					//if (YUV.y >= 127 && YUV.y <= 129 && YUV.z >= 127 && YUV.z <= 129) { YUV.y = 128;yuv.y = 128.0 / 255.0; YUV.z = 128;yuv.z = 128.0 / 255.0; }
					yuv = hdr_to_srgb(yuvConverter, yuv, byte2(YUV.y, YUV.z), bestResult->aspect, bestResult->coefMatrix, bestResult->gamma, bestResult->gammaHLG, bestResult->nits, bestResult->altConvert, bestResult->altPrimariesToSrgb, bestResult->bt2020Range, bestResult->signal, bestResult->coloredAspectMode, bestResult->colorAspect);

					if (bestResult->lchEnabled)
					{
						//yuv *= 255.0;
						doToneMapping(bestResult->lchPrimaries, yuv);
						//yuv /= 255.0;
					}
				}
				else
				{
					yuv = yuvConverter->toRgb((bestResult->signal.isSourceP010) ? YuvConverter::COLOR_RANGE::LIMITED : bestResult->signal.range, bestResult->coef, yuv);
				}

				byte3 result = round_to_0_255<byte3>(yuv * 255.0);
				uint32_t ind_lutd = LUT_INDEX(y, u, v);
				lut[ind_lutd] = result.x;
				lut[ind_lutd + 1] = result.y;
				lut[ind_lutd + 2] = result.z;
			}
}

QString LutCalibrator::CreateLutFile(Logger* _log, QString _rootPath, BestResult* bestResult, std::vector<std::vector<std::vector<CapturedColor>>>* all)
{
	// write LUT table
	QString fileName = QString("%1%2").arg(_rootPath).arg("/lut_lin_tables.3d.tmp");
	QString finalFileName = QString("%1%2").arg(_rootPath).arg("/lut_lin_tables.3d");
	std::fstream file;
	file.open(fileName.toStdString(), std::ios::trunc | std::ios::out | std::ios::binary);

	if (!file.is_open())
	{
		return QString("Could not open LUT file for writing: %1").arg(fileName);
	}
	else
	{
		MemoryBuffer<uint8_t> _lut;
		YuvConverter yuvConverter;
		QThreadPool threadPool;

		std::remove(finalFileName.toStdString().c_str());

		Info(_log, "Writing LUT to temp file: %s", QSTRING_CSTR(fileName));
		Info(_log, "Number of threads for LUT creator: %i", threadPool.maxThreadCount());

		_lut.resize(LUT_FILE_SIZE);

		for (int phase = 0; phase < 3; phase++)
		{
			const int vDelta = std::ceil(256.0 / threadPool.maxThreadCount());

			for (int v = 0; v <= 255; v += vDelta)
			{
				auto worker = new CreateLutWorker(v, std::min(v + vDelta, 256), phase, &yuvConverter, bestResult, _lut.data());
				threadPool.start(worker);
			}
			threadPool.waitForDone();

			if (phase == 1 && all != nullptr)
			{
				for (int r = 0; r < SCREEN_COLOR_DIMENSION; r++)
					for (int g = 0; g < SCREEN_COLOR_DIMENSION; g++)
						for (int b = 0; b < SCREEN_COLOR_DIMENSION; b++)
						{
							auto& sample = (*all)[r][g][b];

							auto list = sample.getInputYUVColors();
							for(auto item = list.begin(); item != list.end(); ++item)
							{
								auto ind_lutd = LUT_INDEX(((uint32_t)(*item).first.x), ((uint32_t)(*item).first.y), ((uint32_t)(*item).first.z));
								(*item).first = byte3{ _lut.data()[ind_lutd], _lut.data()[ind_lutd + 1], _lut.data()[ind_lutd + 2] };
								(*item).second = sample.getSourceError(static_cast<int3>((*item).first));
							}

							list.sort([](const std::pair<byte3, int>& a, const std::pair<byte3, int>& b) { return a.second < b.second; });

							for (auto item = list.begin(); item != list.end(); ++item)
							{
								sample.setFinalRGB((*item).first);
							}
						}
			}

			file.write(reinterpret_cast<char*>(_lut.data()), _lut.size());
		}
		file.flush();
		file.close();
	}	

	if (std::rename(fileName.toStdString().c_str(), finalFileName.toStdString().c_str()))
	{
		return QString("Could not rename %1 to %2").arg(fileName).arg(finalFileName);
	}

	return QString();
}

void LutCalibrator::setupWhitePointCorrection()
{	
	

	//for (const auto& coeff : YuvConverter::knownCoeffs)
	{
		/*
		QString selected;
		double min = std::numeric_limits<double>::max();
		for (int w = WHITE_POINT_D65; w < WHITE_POINT_XY.size(); w++)
		{
			const vec<double, 2>& TEST_WHITE = WHITE_POINT_XY[w];
			
			auto convert_bt2020_to_XYZ = to_XYZ<double>(PRIMARIES[w][0], PRIMARIES[w][1], PRIMARIES[w][2], TEST_WHITE);
			auto white_XYZ = mul(convert_bt2020_to_XYZ, whiteLinRGB);
			auto white_xy = from_XYZ_to_xy(white_XYZ);
			auto difference = TEST_WHITE - white_xy;
			auto distance = length2((TEST_WHITE - white_xy) * 1000000);
			if (distance < min)
			{
				min = distance;
				selected = yuvConverter.coefToString(YuvConverter::YUV_COEFS(coef)) + " => ";
				selected += (w == WHITE_POINT_D65) ? "D65" : ((w == WHITE_POINT_DCI_P3) ? "DCI_P3" : "unknowm");
				selected += QString(" (x: %1, y: %2)").arg(TEST_WHITE.x, 0, 'f', 3).arg(TEST_WHITE.y, 0, 'f', 3);
				calibration.inputBT2020toXYZ[coef] = convert_bt2020_to_XYZ;
			}
		}
		Debug(_log, QSTRING_CSTR(selected));
		*/
	}
}

void LutCalibrator::calibrate()
{
	#ifndef NDEBUG
		sendReport(_log, _yuvConverter->toString());
	#endif

	if (_defaultComp == hyperhdr::COMP_VIDEOGRABBER)
	{
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_VIDEOGRABBER, -1, false);	
	}
	if (_defaultComp == hyperhdr::COMP_FLATBUFSERVER)
	{
		emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_FLATBUFSERVER, -1, false);
	}
	
	_capturedColors->finilizeBoard();


	sendReport(_log, "Captured colors:\r\n" +
				generateReport(false));

	calibration();

	if (_forcedExit)
		return;

	sendReport(_log, "Calibrated:\r\n" +
		generateReport(true));

	notifyCalibrationFinished();

	printReport();
}


void LutCalibrator::capturedPrimariesCorrection(ColorSpaceMath::HDR_GAMMA gamma, double gammaHLG, double nits, int coef, linalg::mat<double, 3, 3>& convert_bt2020_to_XYZ, linalg::mat<double, 3, 3>& convert_XYZ_to_corrected, bool printDebug)
{
	std::vector<CapturedColor> capturedPrimaries{
		_capturedColors->all[SCREEN_COLOR_DIMENSION - 1][0][0], //red
		_capturedColors->all[0][SCREEN_COLOR_DIMENSION - 1][0], // green
		_capturedColors->all[0][0][SCREEN_COLOR_DIMENSION - 1], // blue
		_capturedColors->all[SCREEN_COLOR_DIMENSION - 1][SCREEN_COLOR_DIMENSION - 1][SCREEN_COLOR_DIMENSION - 1] //white
	};
	std::vector<double3> actualPrimaries;

	for (auto& c : capturedPrimaries)
	{
		auto yuv = c.yuv();

		if (gamma == HDR_GAMMA::P010)
		{			
			unpackP010(yuv);
		}

		auto a = _yuvConverter->toRgb(_capturedColors->getRange(), YuvConverter::YUV_COEFS(coef), yuv);

		if (gamma == ColorSpaceMath::HDR_GAMMA::PQ || gamma == ColorSpaceMath::HDR_GAMMA::P010)
		{
			a = PQ_ST2084(10000.0 / nits, a);
		}
		else if (gamma == ColorSpaceMath::HDR_GAMMA::HLG)
		{
			a = OOTF_HLG(inverse_OETF_HLG(a), gammaHLG) * nits;
		}
		else if (gamma == ColorSpaceMath::HDR_GAMMA::BT2020inSRGB)
		{
			a = srgb_nonlinear_to_linear(a);
		}
		else if (gamma == HDR_GAMMA::PQinSRGB)
		{
			a = srgb_linear_to_nonlinear(a);
			a = PQ_ST2084(10000.0 / nits, a);
		}
		actualPrimaries.push_back(a);
	}

	constexpr linalg::vec<double, 2> bt2020_red_xy(0.708, 0.292);
	constexpr linalg::vec<double, 2> bt2020_green_xy(0.17, 0.797);
	constexpr linalg::vec<double, 2> bt2020_blue_xy(0.131, 0.046);
	constexpr linalg::vec<double, 2> bt2020_white_xy(0.3127, 0.3290);
	constexpr double3x3 bt2020_to_XYZ = to_XYZ(bt2020_red_xy, bt2020_green_xy, bt2020_blue_xy, bt2020_white_xy);

	convert_bt2020_to_XYZ = bt2020_to_XYZ;

	double2 sRgb_red_xy = { 0.64f, 0.33f };
	double2 sRgb_green_xy = { 0.30f, 0.60f };
	double2 sRgb_blue_xy = { 0.15f, 0.06f };
	double2 sRgb_white_xy = { 0.3127f, 0.3290f };

	double3 actual_red_xy(actualPrimaries[0]);
	actual_red_xy = linalg::mul(convert_bt2020_to_XYZ, actual_red_xy);
	sRgb_red_xy = XYZ_to_xy(actual_red_xy);

	double3 actual_green_xy(actualPrimaries[1]);
	actual_green_xy = mul(convert_bt2020_to_XYZ, actual_green_xy);
	sRgb_green_xy = XYZ_to_xy(actual_green_xy);

	double3 actual_blue_xy(actualPrimaries[2]);
	actual_blue_xy = mul(convert_bt2020_to_XYZ, actual_blue_xy);
	sRgb_blue_xy = XYZ_to_xy(actual_blue_xy);

	double3 actual_white_xy(actualPrimaries[3]);
	actual_white_xy = mul(convert_bt2020_to_XYZ, actual_white_xy);
	sRgb_white_xy = XYZ_to_xy(actual_white_xy);

	mat<double, 3, 3> convert_sRgb_to_XYZ;
	convert_sRgb_to_XYZ = to_XYZ(sRgb_red_xy, sRgb_green_xy, sRgb_blue_xy, sRgb_white_xy);

	convert_XYZ_to_corrected = inverse(convert_sRgb_to_XYZ);

	if (printDebug)
	{
		double2 sRgbR = { 0.64f, 0.33f };
		double2 sRgbG = { 0.30f, 0.60f };
		double2 sRgbB = { 0.15f, 0.06f };
		double2 sRgbW = { 0.3127f, 0.3290f };

		auto dr = linalg::angle(sRgb_red_xy - sRgb_white_xy, sRgbR - sRgbW);
		auto dg = linalg::angle(sRgb_green_xy - sRgb_white_xy, sRgbG - sRgbW);
		auto db = linalg::angle(sRgb_blue_xy - sRgb_white_xy, sRgbB - sRgbW);

		Debug(_log, "--------------------------------- Actual PQ primaries for YUV coefs: %s ---------------------------------", QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef))));
		Debug(_log, "r: (%.3f, %.3f, a: %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f) vs wide(%.3f, %.3f)", sRgb_red_xy.x, sRgb_red_xy.y, dr, 0.64f, 0.33f, 0.708f, 0.292f, 0.7350f, 0.2650f);
		Debug(_log, "g: (%.3f, %.3f, a: %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f) vs wide(%.3f, %.3f)", sRgb_green_xy.x, sRgb_green_xy.y, dg, 0.30f, 0.60f, 0.17f, 0.797f, 0.1150f, 0.8260f);
		Debug(_log, "b: (%.3f, %.3f, a: %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f) vs wide(%.3f, %.3f)", sRgb_blue_xy.x, sRgb_blue_xy.y, db, 0.15f, 0.06f, 0.131f, 0.046f, 0.1570f, 0.0180f);
		Debug(_log, "w: (%.3f, %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f) vs wide(%.3f, %.3f)", sRgb_white_xy.x, sRgb_white_xy.y, 0.3127f, 0.3290f, 0.3127f, 0.3290f, 0.3127f, 0.3290f);
	}
}


bool LutCalibrator::setTestData()
{
	std::vector<std::vector<int>> capturedData;

	// asssign your test data from calibration_captured_yuv.txt to testData here


	// verify
	if (capturedData.size() != SCREEN_COLOR_DIMENSION * SCREEN_COLOR_DIMENSION * SCREEN_COLOR_DIMENSION)
		return false;

	auto iter = capturedData.begin();
	for (int r = 0; r < SCREEN_COLOR_DIMENSION; r++)
		for (int g = 0; g < SCREEN_COLOR_DIMENSION; g++)
			for (int b = 0; b < SCREEN_COLOR_DIMENSION; b++, ++iter)
			{
				auto& sample = _capturedColors->all[r][g][b];
				int R = std::min(r * SCREEN_COLOR_STEP, 255);
				int G = std::min(g * SCREEN_COLOR_STEP, 255);
				int B = std::min(b * SCREEN_COLOR_STEP, 255);
				sample.setSourceRGB(byte3(R, G, B));

				const auto& colors = (*iter);

				for (int i = 0; i < static_cast<int>(colors.size()); i += 4)
				{
					for (int j = 0; j < colors[i]; j++)
					{
						sample.addColor(ColorRgb(colors[i+1], colors[i+2], colors[i+3]));
					}
				}				
				
				sample.calculateFinalColor();
			}
	if (_capturedColors->all[0][0][0].Y() > SCREEN_YUV_RANGE_LIMIT || _capturedColors->all[0][0][0].Y() < 255 - SCREEN_YUV_RANGE_LIMIT)
	{
		_capturedColors->setRange(YuvConverter::LIMITED);
	}
	else
	{
		_capturedColors->setRange(YuvConverter::FULL);
	}

	return true;
}

void LutCalibrator::CreateDefaultLut(QString filepath)
{
	BestResult bestResult;
	bestResult.coef = YuvConverter::YUV_COEFS(1);
	bestResult.coefMatrix = double4x4{ {1.16438356164, 1.16438356164, 1.16438356164, 0}, {-4.43119136738e-17, -0.387076923077, 2.02178571429, 0}, {1.58691964286, -0.821942994505, -1.77247654695e-16, 0}, {-0.869630789302, 0.53382122535, -1.08791650359, 1} };
	bestResult.coefDelta = double2{ 0.004, -0.002 };
	bestResult.coloredAspectMode = 3;
	bestResult.colorAspect = std::pair<double3, double3>(double3{ 1.08301468119, 1.00959155269, 1.10863646026 }, double3{ 1.06441799482, 1.00700595321, 1.06831049137 });
	bestResult.aspect = double3{ 0.9925, 1, 1.035 };
	bestResult.bt2020Range = 0;
	bestResult.altConvert = 1;
	bestResult.altPrimariesToSrgb = double3x3{ {1.62995144161, -0.159968936426, -0.0191389994477}, {-0.556261837808, 1.17281602107, -0.104271698501}, {-0.0736896038051, -0.0128470846442, 1.12341069795} };
	bestResult.gamma = ColorSpaceMath::HDR_GAMMA(0);
	bestResult.gammaHLG = 0.000000;
	bestResult.lchEnabled = 0;
	bestResult.lchPrimaries = LchLists{
				std::list<double4>{

				},

				std::list<double4>{

				},

				std::list<double4>{

				},
	};
	bestResult.nits = 229.625687;
	bestResult.signal.range = YuvConverter::COLOR_RANGE(2);
	bestResult.signal.yRange = 0.858824;
	bestResult.signal.upYLimit = 0.572549;
	bestResult.signal.downYLimit = 0.062745;
	bestResult.signal.yShift = 0.062745;
	bestResult.signal.isSourceP010 = 0;
	bestResult.minError = 212;

	auto worker = new DefaultLutCreatorWorker(bestResult, filepath);
	QThreadPool::globalInstance()->start(worker);
}

void DefaultLutCreatorWorker::run()
{
	QString errorMessage = LutCalibrator::CreateLutFile(log, path, &bestResult, nullptr);
	if (!errorMessage.isEmpty())
	{
		Error(log, "Error while creating LUT: %s", QSTRING_CSTR(errorMessage));
	}
	else
	{
		Info(log, "The default LUT has been created: %s/lut_lin_tables.3d", QSTRING_CSTR(path));
	}
}
