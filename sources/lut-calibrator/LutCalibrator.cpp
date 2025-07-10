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
#include <led-strip/ColorSpaceCalibration.h>
#include <lut-calibrator/ColorSpace.h>
#include <lut-calibrator/YuvConverter.h>
#include <lut-calibrator/BoardUtils.h>
#include <utils-image/utils-image.h>
#include <lut-calibrator/VectorHelper.h>
#include <fstream>
#include <lut-calibrator/CalibrationWorker.h>

using namespace ColorSpaceMath;
using namespace BoardUtils;


#define LUT_FILE_SIZE 50331648
#define LUT_INDEX(y,u,v) ((y + (u<<8) + (v<<16))*3)

/////////////////////////////////////////////////////////////////
//                          HELPERS                            //
/////////////////////////////////////////////////////////////////
struct MappingPrime;
static void capturedPrimariesCorrection(Logger* _log, ColorSpaceMath::HDR_GAMMA gamma, double gammaHLG, double nits, int coef, glm::dmat3& convert_bt2020_to_XYZ, glm::dmat3& convert_XYZ_to_corrected, std::shared_ptr<YuvConverter>& _yuvConverter, std::shared_ptr<BoardUtils::CapturedColors>& _capturedColors, bool printDebug = false);

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
						.arg(vecToString(ColorSpaceMath::to_byte3(rgbBT709)), 12)
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
	double max = std::max(glm::compMax(pLow), glm::compMax(pHigh));

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


static double3 hdr_to_srgb(const YuvConverter* _yuvConverter, double3 yuv, const byte2& UV, const double3& aspect, const double4x4& coefMatrix, ColorSpaceMath::HDR_GAMMA gamma, double gammaHLG, double nits, int altConvert, const double3x3& bt2020_to_sRgb, int tryBt2020Range, const BestResult::Signal& signal, int colorAspectMode, const std::pair<double3, double3>& colorAspect)
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
			srgb = bt2020_to_sRgb * e;
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

	ColorSpaceMath::trim01(srgb);

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
			QString gammaString = ColorSpaceMath::gammaToString(HDR_GAMMA(gamma));
			QString coefString = yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef));
			if (HDR_GAMMA(gamma) == ColorSpaceMath::HDR_GAMMA::HLG)
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
										auto red = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::RED].first, sampleColors[SampleColor::RED].second, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										auto green = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::GREEN].first, sampleColors[SampleColor::GREEN].second, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										auto blue = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::BLUE].first, sampleColors[SampleColor::BLUE].second, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										colorAspect.first = double3{ 1.0 / red.x, 1.0 / green.y, 1.0 / blue.z };

										red = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::LOW_RED].first, sampleColors[SampleColor::LOW_RED].second, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										green = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::LOW_GREEN].first, sampleColors[SampleColor::LOW_GREEN].second, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										blue = hdr_to_srgb(yuvConverter, sampleColors[SampleColor::LOW_BLUE].first, sampleColors[SampleColor::LOW_BLUE].second, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, 0, colorAspect);
										colorAspect.second = double3{ 0.5 / red.x, 0.5 / green.y, 0.5 / blue.z };
									}

									long long int currentError = 0;

									for (auto v = vertex.begin(); v != vertex.end(); ++v)
									{
										auto& sample = *(*v).first;

										auto minError = MAX_CALIBRATION_ERROR;

										if (sample.U() == 128 && sample.V() == 128)
										{
											(*v).second = hdr_to_srgb(yuvConverter, sample.yuv(), byte2{ sample.U(), sample.V() }, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, coloredAspectMode, colorAspect);
											auto SRGB = to_int3((*v).second * 255.0);
											minError = sample.getSourceError(SRGB);
										}
										else
										{											
											auto sampleList = sample.getInputYuvColors();
											for (auto iter = sampleList.cbegin(); iter != sampleList.cend(); ++iter)
											{
												auto srgb = hdr_to_srgb(yuvConverter, (*iter).first, byte2{ sample.U(), sample.V() }, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, NITS, altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult.signal, coloredAspectMode, colorAspect);

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
											lcHError += (*sample).first->getSourceError((int3)(to_byte3(correctedRGB * 255.0)));
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
										bestResult.gamma = HDR_GAMMA(gamma);
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
											QSTRING_CSTR(gammaToString(HDR_GAMMA(gamma))),
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

	for (int gamma = (precise) ? (bestResult->gamma) : ((bestResult->signal.isSourceP010) ? HDR_GAMMA::P010 : HDR_GAMMA::PQ);
			 gamma <= HDR_GAMMA::P010; gamma++)
	{
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

				capturedPrimariesCorrection(_log, HDR_GAMMA(gamma), gammaHLG, NITS, coef, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb, _yuvConverter, _capturedColors);
				auto bt2020_to_sRgb = convert_XYZ_to_sRgb * convert_bt2020_to_XYZ;

				printf("Processing gamma: %s, gammaHLG: %f, coef: %s. Current best gamma: %s, gammaHLG: %f, coef: %s (d:%s). Score: %.3f\n",
					QSTRING_CSTR(gammaToString(HDR_GAMMA(gamma))), gammaHLG, QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef))),
					QSTRING_CSTR(gammaToString(HDR_GAMMA(bestResult->gamma))), bestResult->gammaHLG, QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(bestResult->coef))),
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

		capturedPrimariesCorrection(_log, HDR_GAMMA(bestResult->gamma), bestResult->gammaHLG, bestResult->nits, bestResult->coef, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb, _yuvConverter, _capturedColors, true);
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
			auto aa = from_XYZ_to_sRGB(lch_to_xyz(c.org) / 100.0) * 255.0;
			auto bb = from_XYZ_to_sRGB(lch_to_xyz(c.real) / 100.0) * 255.0;
			info.append(QString("%1 | %2 | %3 | %4 | %5 %6").arg(vecToString(sample.getSourceRGB()), 12).
				arg(vecToString(c.org)).
				arg(vecToString(c.real)).				
				arg(vecToString(c.delta)).
				arg(vecToString(to_byte3(aa))).
				arg(vecToString(to_byte3(bb))));

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
					YUV = to_byte3(yuv * 255.0);
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

				byte3 result = to_byte3(yuv * 255.0);
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


static void capturedPrimariesCorrection(Logger* _log, ColorSpaceMath::HDR_GAMMA gamma, double gammaHLG, double nits, int coef, glm::dmat3& convert_bt2020_to_XYZ, glm::dmat3& convert_XYZ_to_corrected, std::shared_ptr<YuvConverter>& _yuvConverter, std::shared_ptr<BoardUtils::CapturedColors>& _capturedColors, bool printDebug)
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

	constexpr double2 bt2020_red_xy(0.708, 0.292);
	constexpr double2 bt2020_green_xy(0.17, 0.797);
	constexpr double2 bt2020_blue_xy(0.131, 0.046);
	constexpr double2 bt2020_white_xy(0.3127, 0.3290);
	double3x3 bt2020_to_XYZ = to_XYZ(bt2020_red_xy, bt2020_green_xy, bt2020_blue_xy, bt2020_white_xy);

	convert_bt2020_to_XYZ = bt2020_to_XYZ;

	double2 sRgb_red_xy = { 0.64f, 0.33f };
	double2 sRgb_green_xy = { 0.30f, 0.60f };
	double2 sRgb_blue_xy = { 0.15f, 0.06f };
	double2 sRgb_white_xy = { 0.3127f, 0.3290f };

	double3 actual_red_xy(actualPrimaries[0]);
	actual_red_xy = convert_bt2020_to_XYZ * actual_red_xy;
	sRgb_red_xy = XYZ_to_xy(actual_red_xy);

	double3 actual_green_xy(actualPrimaries[1]);
	actual_green_xy = convert_bt2020_to_XYZ * actual_green_xy;
	sRgb_green_xy = XYZ_to_xy(actual_green_xy);

	double3 actual_blue_xy(actualPrimaries[2]);
	actual_blue_xy = convert_bt2020_to_XYZ * actual_blue_xy;
	sRgb_blue_xy = XYZ_to_xy(actual_blue_xy);

	double3 actual_white_xy(actualPrimaries[3]);
	actual_white_xy = convert_bt2020_to_XYZ * actual_white_xy;
	sRgb_white_xy = XYZ_to_xy(actual_white_xy);

	double3x3 convert_sRgb_to_XYZ;
	convert_sRgb_to_XYZ = to_XYZ(sRgb_red_xy, sRgb_green_xy, sRgb_blue_xy, sRgb_white_xy);

	convert_XYZ_to_corrected = inverse(convert_sRgb_to_XYZ);

	if (printDebug)
	{
		double2 sRgbR = { 0.64f, 0.33f };
		double2 sRgbG = { 0.30f, 0.60f };
		double2 sRgbB = { 0.15f, 0.06f };
		double2 sRgbW = { 0.3127f, 0.3290f };

		auto dr = glm::angle(sRgb_red_xy - sRgb_white_xy, sRgbR - sRgbW);
		auto dg = glm::angle(sRgb_green_xy - sRgb_white_xy, sRgbG - sRgbW);
		auto db = glm::angle(sRgb_blue_xy - sRgb_white_xy, sRgbB - sRgbW);

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
	capturedData = {
		{ 36,16,128,128},{ 36,17,133,128},{ 36,20,138,128},{ 36,23,144,128},{ 36,26,148,127},{ 36,29,154,127},{ 36,32,159,128},{ 36,35,164,128},{ 36,38,170,128},{ 36,40,175,128},{ 36,44,180,128},{ 36,47,184,128},{ 36,51,189,128},{ 36,54,194,128},{ 36,58,199,128},{ 36,62,204,128},{ 36,65,208,128},
		{ 36,24,124,126},{ 36,26,129,125},{ 36,27,134,125},{ 36,29,140,125},{ 36,31,145,125},{ 36,34,150,125},{ 36,37,156,126},{ 36,40,162,126},{ 36,43,167,126},{ 36,45,172,127},{ 36,49,177,126},{ 36,52,182,127},{ 36,56,187,127},{ 36,59,192,127},{ 36,62,197,127},{ 36,66,202,127},{ 24,69,206,127, 12,68,206,127},
		{ 36,33,122,124},{ 36,34,125,124},{ 36,35,130,124},{ 36,38,136,123},{ 36,39,141,123},{ 36,42,147,123},{ 36,45,152,123},{ 36,47,157,124},{ 36,50,163,124},{ 36,52,168,124},{ 36,55,173,124},{ 36,58,178,125},{ 36,61,184,125},{ 36,65,189,125},{ 36,67,194,125},{ 36,71,199,125},{ 36,74,203,125},
		{ 36,42,119,123},{ 36,43,121,122},{ 36,44,126,122},{ 36,46,131,122},{ 36,48,137,122},{ 36,50,142,122},{ 36,53,148,122},{ 36,55,153,122},{ 36,58,158,122},{ 36,60,164,122},{ 36,62,169,122},{ 36,65,174,122},{ 36,68,180,123},{ 36,71,185,123},{ 36,74,190,123},{ 36,77,195,123},{ 36,80,200,123},
		{ 36,51,115,121},{ 36,52,118,121},{ 36,53,122,121},{ 36,55,127,120},{ 36,57,133,120},{ 36,59,138,120},{ 36,61,144,120},{ 36,63,149,120},{ 36,66,154,120},{ 24,67,160,120, 12,68,160,120},{ 36,70,165,120},{ 36,73,171,120},{ 36,75,176,121},{ 36,78,181,121},{ 36,81,186,121},{ 36,84,191,121},{ 36,86,196,121},
		{ 36,60,113,120},{ 36,62,115,120},{ 36,62,118,119},{ 36,64,123,119},{ 36,66,128,119},{ 36,67,134,119},{ 36,70,139,119},{ 36,71,145,118},{ 36,74,150,118},{ 36,76,155,118},{ 36,78,161,118},{ 36,81,166,119},{ 36,83,172,119},{ 36,86,177,119},{ 36,88,182,119},{ 36,91,187,119},{ 36,94,192,119},
		{ 36,69,110,119},{ 36,70,111,118},{ 36,72,115,118},{ 36,73,119,118},{ 36,75,124,117},{ 24,76,129,117, 12,77,129,117},{ 28,78,134,117, 8,79,134,117},{ 36,80,140,117},{ 36,82,145,117},{ 36,85,150,117},{ 24,86,156,117, 12,87,156,117},{ 36,89,161,117},{ 36,91,167,117},{ 36,94,172,117},{ 36,96,178,117},{ 36,99,184,117},{ 36,101,188,117},
		{ 36,78,107,117},{ 36,80,108,117},{ 36,81,111,117},{ 36,82,116,116},{ 36,84,120,116},{ 36,85,125,116},{ 36,87,131,116},{ 24,89,136,116, 12,88,136,116},{ 36,91,141,115},{ 36,93,147,115},{ 36,95,152,115},{ 36,97,157,115},{ 36,99,163,115},{ 36,102,168,116},{ 36,104,174,115},{ 36,107,179,116},{ 28,108,184,116, 8,109,184,116},
		{ 36,88,104,116},{ 36,89,106,116},{ 36,90,108,116},{ 36,91,112,115},{ 36,93,116,115},{ 36,94,121,115},{ 36,96,126,114},{ 36,98,132,114},{ 36,100,137,114},{ 36,102,142,114},{ 36,103,148,114},{ 36,106,153,114},{ 36,108,158,114},{ 36,110,164,114},{ 36,112,169,114},{ 36,115,174,114},{ 36,117,180,114},
		{ 32,97,101,115, 4,98,101,115},{ 36,98,102,115},{ 36,99,105,114},{ 36,100,108,114},{ 36,102,112,114},{ 36,103,117,114},{ 36,105,122,113},{ 36,107,127,113},{ 36,108,132,113},{ 36,111,137,113},{ 36,112,143,112},{ 36,115,148,112},{ 36,116,153,112},{ 36,119,159,112},{ 36,121,165,112},{ 36,123,171,112},{ 36,125,175,112},
		{ 36,107,98,113},{ 36,107,100,113},{ 36,109,102,113},{ 36,109,105,113},{ 36,111,109,112},{ 36,112,113,112},{ 36,114,118,112},{ 36,116,123,112},{ 36,117,128,111},{ 36,119,134,111},{ 36,121,139,111},{ 36,123,144,111},{ 36,125,150,111},{ 36,127,155,111},{ 28,129,161,111, 8,130,161,111},{ 36,131,166,111},{ 28,134,171,111, 8,133,171,111},
		{ 36,116,96,112},{ 36,116,97,112},{ 36,118,99,112},{ 36,118,102,112},{ 36,120,106,111},{ 36,121,110,111},{ 36,123,114,111},{ 36,125,119,110},{ 36,126,124,110},{ 36,128,129,110},{ 36,129,135,110},{ 36,132,140,110},{ 36,134,145,109},{ 36,136,151,109},{ 36,138,156,109},{ 36,140,161,109},{ 36,142,167,109},
		{ 36,125,93,111},{ 28,126,94,111, 8,125,94,111},{ 28,126,96,111, 8,127,96,111},{ 28,127,98,110, 8,128,98,110},{ 36,129,102,110},{ 36,131,106,110},{ 36,132,110,109},{ 36,134,115,109},{ 36,135,120,109},{ 36,137,125,109},{ 24,139,130,108, 12,138,130,108},{ 24,141,135,108, 12,140,135,108},{ 36,143,141,108},{ 36,144,147,108},{ 36,147,152,108},{ 24,149,158,108, 12,148,158,108},{ 36,151,162,108},
		{ 36,134,90,110},{ 36,135,91,110},{ 36,136,93,109},{ 36,137,95,109},{ 36,138,99,109},{ 36,140,103,109},{ 36,141,107,108},{ 36,143,111,108},{ 36,144,116,108},{ 36,146,121,107},{ 36,148,126,107},{ 36,150,131,107},{ 36,152,137,107},{ 36,153,142,107},{ 36,155,147,106},{ 36,157,153,106},{ 36,159,158,106},
		{ 36,144,88,108},{ 36,144,88,108},{ 36,145,90,108},{ 36,147,92,108},{ 24,148,95,108, 12,147,95,108},{ 36,149,99,107},{ 36,150,103,107},{ 36,152,107,107},{ 36,153,112,106},{ 36,155,117,106},{ 36,157,122,106},{ 36,158,127,106},{ 36,161,132,105},{ 36,162,138,106},{ 36,164,143,105},{ 36,166,148,105},{ 36,168,154,105},
		{ 36,153,85,107},{ 36,154,85,107},{ 36,155,87,107},{ 36,156,89,107},{ 36,156,92,106},{ 36,158,95,106},{ 36,159,99,106},{ 36,161,103,106},{ 36,163,108,105},{ 36,164,113,105},{ 36,166,118,105},{ 36,167,124,105},{ 36,169,129,104},{ 36,171,134,104},{ 36,173,139,104},{ 36,174,144,104},{ 36,176,149,104},
		{ 36,162,82,106},{ 36,162,83,106},{ 36,163,84,106},{ 36,164,86,106},{ 36,165,89,105},{ 36,166,92,105},{ 36,168,96,105},{ 36,170,100,104},{ 36,171,105,104},{ 36,173,109,104},{ 36,174,114,104},{ 36,176,119,104},{ 36,177,124,103},{ 36,180,129,103},{ 36,181,134,103},{ 36,183,140,103},{ 36,185,145,103},
		{ 36,18,126,131},{ 36,21,132,131},{ 36,23,137,130},{ 36,26,142,129},{ 36,28,147,129},{ 36,31,153,129},{ 36,34,158,129},{ 36,36,164,129},{ 36,39,169,129},{ 36,42,174,129},{ 36,45,179,129},{ 36,48,184,129},{ 36,52,189,129},{ 36,55,193,129},{ 36,59,198,129},{ 36,63,203,129},{ 36,66,208,129},
		{ 36,26,123,128},{ 36,29,128,128},{ 36,29,133,127},{ 36,32,138,127},{ 36,33,144,127},{ 36,36,149,127},{ 36,39,155,127},{ 36,41,160,127},{ 36,44,166,127},{ 36,47,171,127},{ 36,50,176,127},{ 36,53,181,127},{ 36,57,186,127},{ 36,60,191,127},{ 36,63,196,128},{ 36,67,201,127},{ 36,70,206,128},
		{ 36,35,121,125},{ 36,36,124,125},{ 36,37,129,125},{ 36,39,134,125},{ 36,42,140,125},{ 36,44,145,124},{ 36,47,150,124},{ 36,49,157,124},{ 36,51,162,125},{ 36,53,168,125},{ 36,57,173,125},{ 36,59,178,125},{ 36,62,183,125},{ 36,66,188,125},{ 36,68,193,126},{ 36,72,198,126},{ 36,75,202,126},
		{ 36,44,118,124},{ 36,45,121,124},{ 36,46,125,123},{ 36,48,131,123},{ 36,50,136,123},{ 36,52,141,122},{ 36,54,147,123},{ 24,57,152,122, 12,56,152,122},{ 36,59,157,123},{ 36,61,163,123},{ 36,64,169,123},{ 36,67,174,123},{ 36,69,179,123},{ 36,72,184,123},{ 36,75,189,123},{ 36,78,194,124},{ 36,80,199,124},
		{ 36,53,115,122},{ 36,54,117,122},{ 36,56,121,121},{ 36,57,126,121},{ 36,59,132,121},{ 36,60,137,121},{ 36,62,142,121},{ 36,64,148,121},{ 36,67,153,120},{ 36,68,159,121},{ 36,71,164,121},{ 36,74,169,121},{ 36,76,175,121},{ 36,79,180,121},{ 36,81,185,121},{ 36,85,190,121},{ 36,87,195,122},
		{ 36,61,112,121},{ 36,63,114,120},{ 36,65,117,120},{ 36,65,122,120},{ 36,68,127,119},{ 36,69,133,119},{ 36,71,139,119},{ 36,73,144,119},{ 36,75,149,119},{ 24,77,155,119, 12,78,155,119},{ 36,79,160,119},{ 36,82,166,119},{ 36,84,171,119},{ 36,87,176,119},{ 36,89,182,119},{ 36,92,187,119},{ 36,94,191,119},
		{ 36,71,109,119},{ 36,71,111,119},{ 36,74,114,118},{ 36,74,119,118},{ 36,76,124,118},{ 36,78,128,118},{ 36,80,134,117},{ 36,81,139,117},{ 36,83,145,117},{ 36,86,150,117},{ 36,88,156,117},{ 36,90,161,117},{ 36,92,167,117},{ 36,95,172,117},{ 36,97,177,117},{ 36,100,182,117},{ 36,102,188,118},
		{ 36,80,106,118},{ 36,80,108,117},{ 36,82,111,117},{ 36,83,115,117},{ 36,85,119,117},{ 36,86,124,116},{ 36,89,129,116},{ 36,91,135,116},{ 36,92,140,116},{ 36,94,146,116},{ 36,96,151,115},{ 24,98,157,115, 12,99,157,115},{ 36,100,162,116},{ 36,103,167,115},{ 36,106,173,115},{ 28,108,178,116, 8,107,178,116},{ 28,110,183,116, 8,111,183,116},
		{ 28,89,104,116, 8,90,104,116},{ 36,90,105,116},{ 36,91,107,116},{ 36,92,111,116},{ 36,94,116,115},{ 36,95,121,115},{ 36,97,126,114},{ 36,99,131,114},{ 28,100,136,114, 8,101,136,114},{ 36,103,142,114},{ 36,104,147,114},{ 36,107,152,114},{ 36,109,158,114},{ 28,111,163,114, 8,112,163,114},{ 36,114,169,114},{ 24,115,174,114, 12,116,174,114},{ 36,118,179,114},
		{ 36,98,101,115},{ 36,99,102,115},{ 36,100,105,114},{ 36,101,108,114},{ 36,103,112,114},{ 36,105,117,113},{ 36,106,122,113},{ 36,108,127,113},{ 36,109,132,113},{ 36,111,137,113},{ 36,113,143,113},{ 36,115,148,112},{ 36,118,153,112},{ 36,120,159,112},{ 36,122,164,112},{ 36,124,169,112},{ 36,127,175,112},
		{ 36,108,98,113},{ 36,108,99,113},{ 36,109,101,113},{ 36,110,104,113},{ 36,112,108,113},{ 36,114,113,112},{ 36,115,117,112},{ 36,117,122,112},{ 36,118,127,111},{ 36,120,133,111},{ 28,122,138,111, 8,121,138,111},{ 36,124,143,111},{ 36,126,149,111},{ 36,128,154,111},{ 36,130,159,111},{ 36,132,166,111},{ 24,135,170,111, 12,134,170,111},
		{ 36,117,95,112},{ 28,118,96,112, 8,117,96,112},{ 24,118,99,112, 12,119,99,112},{ 36,120,102,112},{ 36,121,105,111},{ 36,123,109,111},{ 36,124,114,111},{ 36,126,119,110},{ 36,127,124,110},{ 36,129,129,110},{ 36,131,134,110},{ 28,133,140,110, 8,132,140,110},{ 36,135,145,110},{ 36,137,150,109},{ 36,139,156,109},{ 36,141,161,109},{ 36,143,166,109},
		{ 36,126,93,111},{ 24,127,94,111, 12,126,94,111},{ 36,128,95,110},{ 36,129,98,110},{ 36,130,101,110},{ 36,132,105,110},{ 20,133,110,109, 16,132,110,109},{ 36,135,115,109},{ 36,136,120,109},{ 36,138,124,108},{ 36,141,129,108},{ 36,141,135,108},{ 36,144,140,108},{ 36,145,146,108},{ 36,148,151,108},{ 36,149,156,108},{ 36,152,162,108},
		{ 28,136,90,110, 8,135,90,110},{ 36,137,91,109},{ 36,137,92,109},{ 36,139,95,109},{ 36,139,98,109},{ 36,141,102,108},{ 36,142,106,108},{ 36,144,110,108},{ 36,146,115,108},{ 36,147,120,107},{ 36,149,125,107},{ 36,150,130,107},{ 36,152,136,107},{ 36,154,142,107},{ 36,156,147,106},{ 36,158,153,106},{ 36,160,157,106},
		{ 24,144,87,108, 12,145,87,108},{ 36,146,88,108},{ 36,146,89,108},{ 36,148,92,108},{ 36,148,95,108},{ 36,150,98,107},{ 36,151,103,107},{ 36,153,107,107},{ 36,155,112,106},{ 36,156,117,106},{ 36,158,122,106},{ 36,159,127,106},{ 36,161,132,105},{ 36,163,137,106},{ 36,165,143,105},{ 36,167,148,105},{ 36,169,153,105},
		{ 36,154,85,107},{ 36,155,85,107},{ 36,156,86,107},{ 36,157,89,107},{ 36,157,91,106},{ 36,159,95,106},{ 36,161,99,106},{ 36,162,103,106},{ 36,164,108,105},{ 36,165,112,105},{ 36,167,117,105},{ 36,168,122,104},{ 36,170,127,104},{ 36,172,133,104},{ 36,174,138,104},{ 36,176,144,104},{ 36,177,149,104},
		{ 36,163,82,106},{ 36,163,83,106},{ 36,164,84,106},{ 36,165,86,106},{ 36,166,88,105},{ 36,167,92,105},{ 36,169,96,105},{ 36,171,100,104},{ 36,172,104,104},{ 36,174,109,104},{ 36,175,114,104},{ 36,177,119,103},{ 36,178,124,103},{ 36,180,129,103},{ 36,182,134,103},{ 36,184,140,103},{ 36,186,144,102},
		{ 36,24,124,134},{ 36,26,128,134},{ 36,27,134,133},{ 36,29,140,133},{ 36,31,145,132},{ 36,33,151,131},{ 36,36,156,131},{ 36,39,162,131},{ 36,41,167,131},{ 36,44,173,131},{ 36,47,178,131},{ 36,51,183,131},{ 36,54,188,130},{ 36,58,193,130},{ 36,61,198,130},{ 36,64,203,130},{ 36,67,207,130},
		{ 36,30,122,131},{ 36,32,126,131},{ 36,33,131,130},{ 36,35,137,130},{ 36,38,142,130},{ 36,39,148,129},{ 36,42,154,129},{ 36,44,159,129},{ 36,47,165,129},{ 36,49,170,129},{ 36,52,175,128},{ 36,56,180,129},{ 36,58,185,129},{ 36,62,190,128},{ 36,65,195,128},{ 36,68,200,128},{ 36,71,205,128},
		{ 36,38,120,128},{ 36,39,122,128},{ 36,41,128,128},{ 36,43,133,127},{ 36,45,138,127},{ 36,46,144,127},{ 36,49,150,126},{ 36,51,155,126},{ 36,53,160,126},{ 36,55,166,126},{ 24,58,171,126, 12,59,171,126},{ 36,61,177,126},{ 36,64,182,126},{ 36,67,187,126},{ 36,70,192,127},{ 36,73,197,126},{ 36,76,202,127},
		{ 36,46,117,126},{ 36,47,119,125},{ 36,49,124,125},{ 36,50,129,125},{ 36,53,134,125},{ 36,54,140,124},{ 36,56,145,124},{ 36,58,150,124},{ 36,61,157,124},{ 36,64,162,124},{ 36,66,168,124},{ 36,69,173,124},{ 36,71,178,124},{ 36,74,184,124},{ 36,76,189,124},{ 36,79,194,124},{ 36,82,198,124},
		{ 36,54,114,124},{ 36,56,116,123},{ 36,58,120,123},{ 36,59,125,123},{ 36,61,131,123},{ 36,62,136,122},{ 36,65,141,122},{ 36,66,147,122},{ 24,69,152,122, 12,68,152,122},{ 36,71,158,122},{ 36,73,163,122},{ 36,76,169,122},{ 36,78,174,122},{ 36,81,179,122},{ 36,83,184,122},{ 36,86,190,122},{ 36,88,195,122},
		{ 36,64,111,122},{ 36,65,113,122},{ 24,66,117,121, 12,67,117,121},{ 36,67,121,121},{ 36,70,126,121},{ 28,70,132,121, 8,71,132,121},{ 36,73,137,120},{ 36,75,142,120},{ 36,76,148,120},{ 36,79,153,120},{ 36,81,159,120},{ 36,83,164,120},{ 24,85,169,120, 12,86,169,120},{ 36,88,175,120},{ 36,90,180,120},{ 36,93,186,120},{ 36,96,191,120},
		{ 36,73,108,120},{ 36,73,110,120},{ 36,75,113,120},{ 36,76,117,119},{ 36,78,122,119},{ 24,80,127,119, 12,79,127,119},{ 36,81,133,119},{ 36,83,139,118},{ 36,85,144,118},{ 24,87,150,118, 12,88,150,118},{ 36,89,155,118},{ 36,92,160,118},{ 36,94,166,118},{ 36,96,171,118},{ 36,99,177,118},{ 36,101,182,118},{ 36,104,186,118},
		{ 36,82,106,119},{ 36,82,107,118},{ 36,84,110,118},{ 36,85,114,118},{ 36,87,119,117},{ 36,89,124,117},{ 36,90,129,117},{ 36,92,134,117},{ 36,93,139,117},{ 36,96,145,116},{ 36,97,150,116},{ 36,100,156,116},{ 36,101,161,116},{ 36,104,167,116},{ 36,107,172,116},{ 36,109,177,116},{ 36,112,183,116},
		{ 36,91,103,117},{ 36,91,105,117},{ 36,93,107,117},{ 36,93,111,116},{ 36,96,115,116},{ 36,97,120,116},{ 36,99,125,116},{ 36,101,129,115},{ 36,102,135,115},{ 36,104,140,115},{ 36,106,146,115},{ 36,108,151,114},{ 36,111,157,114},{ 36,112,162,114},{ 36,115,168,114},{ 36,117,173,114},{ 36,119,178,114},
		{ 36,100,101,115},{ 36,100,102,116},{ 36,102,104,115},{ 36,103,107,115},{ 36,104,111,115},{ 36,106,116,114},{ 24,107,121,114, 12,108,121,114},{ 28,109,126,114, 8,110,126,114},{ 36,111,132,113},{ 36,113,137,113},{ 36,114,142,113},{ 36,117,148,113},{ 36,119,153,113},{ 36,121,158,112},{ 36,123,164,113},{ 36,125,169,113},{ 28,128,174,113, 8,127,174,113},
		{ 36,109,98,114},{ 36,109,99,114},{ 36,111,101,114},{ 36,112,104,114},{ 36,113,108,113},{ 36,115,112,113},{ 24,116,117,112, 12,117,117,112},{ 32,118,122,112, 4,119,122,112},{ 36,120,127,112},{ 24,121,132,112, 12,122,132,112},{ 36,124,137,111},{ 36,125,143,111},{ 28,128,148,111, 8,127,148,111},{ 36,129,154,111},{ 24,131,159,111, 12,132,159,111},{ 28,133,165,111, 8,134,165,111},{ 36,136,170,111},
		{ 36,118,95,113},{ 36,119,96,112},{ 36,120,98,112},{ 36,121,101,112},{ 36,122,104,112},{ 36,124,108,112},{ 36,125,113,111},{ 36,127,117,111},{ 28,128,123,110, 8,129,123,110},{ 28,130,127,110, 8,131,127,110},{ 32,133,133,110, 4,132,133,110},{ 36,134,138,110},{ 36,136,144,110},{ 36,138,149,110},{ 36,140,154,109},{ 36,142,160,109},{ 36,144,165,109},
		{ 24,128,92,111, 12,127,92,111},{ 36,129,93,111},{ 36,129,95,111},{ 36,130,98,111},{ 36,131,101,110},{ 36,133,105,110},{ 36,134,110,110},{ 36,136,114,109},{ 36,138,119,109},{ 36,139,124,109},{ 36,141,129,109},{ 36,143,134,109},{ 36,145,140,108},{ 28,146,145,108, 8,147,145,108},{ 36,149,150,108},{ 36,151,156,108},{ 36,153,161,108},
		{ 36,137,90,110},{ 36,138,90,110},{ 36,138,92,110},{ 36,140,94,109},{ 36,140,97,109},{ 36,142,101,109},{ 36,143,105,109},{ 36,145,110,108},{ 36,147,115,108},{ 36,148,120,108},{ 36,150,125,107},{ 36,151,130,107},{ 36,154,136,107},{ 36,155,141,107},{ 36,157,146,107},{ 36,160,151,106},{ 36,161,157,106},
		{ 36,146,87,109},{ 36,147,87,108},{ 24,147,89,108, 12,148,89,108},{ 36,149,91,108},{ 36,149,94,108},{ 36,151,98,108},{ 36,153,102,107},{ 36,154,106,107},{ 36,156,111,107},{ 36,157,116,106},{ 36,159,121,106},{ 36,160,126,106},{ 36,162,131,106},{ 24,165,136,106, 12,164,136,106},{ 36,166,142,105},{ 36,168,148,105},{ 36,170,152,105},
		{ 36,155,84,107},{ 36,156,85,107},{ 28,156,86,107, 8,157,86,107},{ 36,158,88,107},{ 32,159,91,107, 4,158,91,107},{ 36,160,94,106},{ 36,162,98,106},{ 36,163,103,106},{ 36,164,107,106},{ 36,166,112,105},{ 36,168,117,105},{ 36,169,122,105},{ 36,171,127,104},{ 36,173,132,104},{ 36,175,138,104},{ 36,177,143,104},{ 36,178,148,104},
		{ 36,164,82,106},{ 36,165,82,106},{ 36,165,83,106},{ 36,166,85,106},{ 36,167,88,106},{ 36,169,91,105},{ 36,170,95,105},{ 36,172,99,104},{ 36,173,103,104},{ 24,174,108,104, 12,175,108,104},{ 36,176,113,104},{ 36,178,118,104},{ 36,180,123,103},{ 24,182,128,103, 12,181,128,103},{ 36,183,133,103},{ 36,185,139,103},{ 36,187,144,102},
		{ 36,29,123,138},{ 36,30,127,137},{ 36,31,132,136},{ 36,33,138,136},{ 36,36,143,135},{ 36,37,149,134},{ 36,40,155,134},{ 36,42,160,134},{ 36,44,165,134},{ 36,47,171,133},{ 36,50,176,133},{ 36,54,181,133},{ 36,56,186,132},{ 36,60,191,132},{ 36,62,196,132},{ 36,66,201,131},{ 36,69,206,132},
		{ 36,35,121,135},{ 36,36,124,134},{ 36,38,129,134},{ 36,39,135,133},{ 36,41,140,133},{ 36,43,145,132},{ 36,45,151,131},{ 36,47,157,131},{ 36,50,162,131},{ 36,52,168,131},{ 36,55,174,131},{ 36,58,179,130},{ 36,61,184,130},{ 36,64,189,130},{ 36,67,194,130},{ 36,70,199,130},{ 36,73,203,130},
		{ 36,41,119,131},{ 36,43,121,131},{ 36,45,126,131},{ 36,46,132,130},{ 36,48,137,130},{ 36,49,142,130},{ 36,52,148,129},{ 36,53,154,129},{ 36,56,159,128},{ 36,59,165,128},{ 36,61,170,128},{ 36,64,175,128},{ 36,66,180,128},{ 36,69,186,128},{ 36,72,191,128},{ 36,75,196,128},{ 36,78,201,128},
		{ 36,50,116,129},{ 36,50,118,128},{ 36,52,123,128},{ 36,54,128,128},{ 36,55,133,127},{ 36,57,138,127},{ 36,59,144,127},{ 36,61,149,126},{ 36,63,155,126},{ 36,66,160,126},{ 36,68,166,126},{ 36,71,171,126},{ 36,73,177,126},{ 36,76,182,126},{ 36,78,187,126},{ 36,81,192,126},{ 24,83,197,126, 12,84,197,126},
		{ 36,58,113,127},{ 36,59,115,126},{ 24,60,119,125, 12,61,119,125},{ 36,61,124,125},{ 36,63,129,125},{ 36,65,134,124},{ 36,67,140,124},{ 36,69,145,124},{ 36,71,150,124},{ 36,73,157,124},{ 36,75,162,124},{ 36,78,168,124},{ 36,80,173,124},{ 36,83,179,123},{ 36,86,184,123},{ 36,88,189,123},{ 36,91,194,123},
		{ 36,67,110,124},{ 36,67,113,124},{ 36,69,116,123},{ 36,70,120,123},{ 36,72,125,123},{ 36,73,131,122},{ 36,75,136,122},{ 36,77,142,122},{ 24,78,147,122, 12,79,147,122},{ 36,81,152,121},{ 36,83,158,121},{ 36,86,163,121},{ 36,87,169,121},{ 36,90,174,121},{ 36,93,179,121},{ 36,95,185,121},{ 36,98,190,121},
		{ 36,75,108,122},{ 36,76,109,122},{ 36,77,112,121},{ 36,78,117,121},{ 36,80,121,121},{ 36,82,127,120},{ 36,83,132,120},{ 36,85,137,120},{ 36,87,142,120},{ 36,89,148,120},{ 36,91,153,119},{ 36,93,159,119},{ 36,96,164,119},{ 36,98,170,119},{ 24,101,175,119, 12,100,175,119},{ 36,102,181,119},{ 36,105,186,119},
		{ 36,84,105,120},{ 24,85,107,120, 12,84,107,120},{ 36,86,109,120},{ 36,87,113,119},{ 36,88,117,119},{ 36,91,122,118},{ 36,92,127,118},{ 36,94,133,118},{ 36,95,139,118},{ 36,97,144,118},{ 36,99,149,117},{ 36,101,155,117},{ 36,104,161,117},{ 36,106,166,117},{ 36,108,171,117},{ 36,110,177,117},{ 36,113,182,117},
		{ 36,93,103,119},{ 36,93,104,118},{ 36,95,106,118},{ 36,96,110,118},{ 36,97,114,117},{ 36,99,119,117},{ 36,100,124,116},{ 36,103,129,116},{ 36,103,134,116},{ 36,106,140,116},{ 36,108,145,116},{ 36,110,150,116},{ 36,112,156,115},{ 36,114,161,115},{ 36,116,167,115},{ 36,118,172,115},{ 36,121,178,115},
		{ 36,102,100,117},{ 36,102,101,117},{ 36,103,103,116},{ 36,105,106,116},{ 36,106,111,116},{ 36,108,115,115},{ 36,109,120,115},{ 36,111,125,115},{ 36,112,130,114},{ 36,114,135,114},{ 36,117,141,114},{ 36,118,146,114},{ 36,120,151,114},{ 36,122,157,113},{ 36,125,162,113},{ 36,126,168,114},{ 36,129,173,113},
		{ 36,111,97,115},{ 36,112,98,115},{ 36,112,100,115},{ 36,114,103,114},{ 36,115,107,114},{ 24,116,111,114, 12,117,111,114},{ 36,118,116,114},{ 36,120,121,113},{ 36,122,126,113},{ 36,123,132,113},{ 36,125,137,112},{ 36,127,142,112},{ 36,129,148,112},{ 36,131,153,112},{ 36,133,159,112},{ 36,135,164,112},{ 36,137,169,112},
		{ 36,120,95,113},{ 36,121,96,113},{ 36,121,97,113},{ 36,123,100,113},{ 24,123,104,113, 12,124,104,113},{ 36,125,108,112},{ 36,127,112,112},{ 36,129,117,111},{ 36,131,122,111},{ 36,132,127,111},{ 36,134,133,111},{ 36,135,138,111},{ 36,138,143,110},{ 36,139,149,110},{ 24,142,154,110, 12,141,154,110},{ 36,144,159,110},{ 36,146,165,110},
		{ 36,129,92,112},{ 36,130,93,112},{ 36,131,94,112},{ 36,132,97,111},{ 36,133,100,111},{ 36,134,104,111},{ 36,136,108,111},{ 36,138,113,110},{ 36,140,118,110},{ 36,141,123,110},{ 36,143,128,109},{ 36,144,133,109},{ 36,146,139,109},{ 36,148,144,109},{ 36,150,149,109},{ 36,152,154,109},{ 36,154,160,108},
		{ 36,138,89,111},{ 36,139,90,111},{ 36,140,91,110},{ 36,141,93,110},{ 36,142,97,110},{ 36,143,101,110},{ 36,145,105,109},{ 36,146,110,109},{ 36,148,114,108},{ 36,149,119,108},{ 36,152,125,108},{ 36,153,129,108},{ 36,155,135,108},{ 36,157,140,107},{ 36,158,146,107},{ 36,161,151,107},{ 36,163,156,107},
		{ 36,147,87,109},{ 36,148,87,109},{ 36,149,88,109},{ 36,150,91,109},{ 36,152,94,108},{ 36,152,97,108},{ 36,154,101,108},{ 36,155,106,108},{ 36,157,110,107},{ 36,158,115,107},{ 36,160,120,107},{ 36,161,125,106},{ 36,164,131,106},{ 36,166,136,106},{ 36,167,141,106},{ 36,169,146,106},{ 36,171,152,105},
		{ 36,156,84,108},{ 36,157,84,108},{ 36,158,86,108},{ 36,159,87,107},{ 36,161,90,107},{ 36,161,94,107},{ 36,163,98,106},{ 36,164,102,106},{ 36,166,106,106},{ 36,167,111,106},{ 36,169,116,105},{ 36,171,121,105},{ 36,173,126,105},{ 36,175,131,105},{ 36,176,137,104},{ 36,178,143,104},{ 36,179,147,104},
		{ 36,165,81,107},{ 36,166,82,107},{ 36,167,83,106},{ 36,168,85,106},{ 36,169,87,106},{ 36,170,90,106},{ 36,171,94,105},{ 36,173,98,105},{ 36,174,103,105},{ 36,176,108,104},{ 36,178,112,104},{ 36,179,118,104},{ 36,181,123,104},{ 24,182,128,103, 12,183,128,103},{ 36,185,133,103},{ 36,186,138,103},{ 36,188,144,103},
		{ 36,34,122,141},{ 36,35,125,140},{ 36,37,130,140},{ 36,38,135,139},{ 36,40,141,138},{ 36,42,147,137},{ 36,44,152,137},{ 36,46,158,137},{ 36,48,164,136},{ 36,51,169,136},{ 24,54,174,136, 12,53,174,136},{ 36,57,179,135},{ 36,59,185,135},{ 36,63,190,134},{ 36,65,195,134},{ 36,68,200,133},{ 36,71,205,133},
		{ 36,40,120,138},{ 36,40,122,138},{ 36,42,127,137},{ 36,43,133,136},{ 36,45,138,136},{ 36,47,144,135},{ 36,49,149,135},{ 36,51,155,134},{ 36,53,160,134},{ 36,56,166,133},{ 36,58,171,133},{ 36,61,177,133},{ 36,64,182,133},{ 36,67,187,132},{ 36,69,192,132},{ 36,72,197,132},{ 36,75,202,132},
		{ 36,46,117,135},{ 24,47,119,134, 12,46,119,134},{ 36,49,124,134},{ 36,49,129,134},{ 36,52,135,133},{ 36,53,140,132},{ 36,55,146,132},{ 36,58,151,131},{ 36,59,157,131},{ 36,62,162,131},{ 36,64,168,131},{ 36,67,174,130},{ 36,69,179,130},{ 36,72,184,130},{ 36,75,190,130},{ 24,77,195,130, 12,78,195,130},{ 36,81,199,129},
		{ 36,54,115,132},{ 36,54,117,132},{ 36,56,121,131},{ 36,57,126,131},{ 36,59,132,130},{ 36,60,137,130},{ 36,62,142,129},{ 36,65,148,129},{ 36,66,153,128},{ 36,69,159,128},{ 36,71,165,128},{ 36,73,170,128},{ 36,75,175,128},{ 36,78,181,128},{ 36,81,186,128},{ 36,84,191,128},{ 36,87,196,127},
		{ 36,61,112,130},{ 36,62,114,129},{ 36,63,118,129},{ 36,64,122,128},{ 36,67,128,128},{ 36,68,133,127},{ 36,70,138,127},{ 36,72,144,127},{ 36,74,149,126},{ 36,76,155,126},{ 36,78,160,126},{ 36,80,166,126},{ 36,83,171,126},{ 36,85,177,126},{ 36,88,182,125},{ 36,90,187,125},{ 36,93,192,125},
		{ 36,70,109,127},{ 36,70,111,127},{ 36,72,114,126},{ 36,73,119,125},{ 36,74,124,125},{ 36,76,129,125},{ 36,78,134,124},{ 36,80,140,124},{ 36,81,145,124},{ 36,84,150,124},{ 36,85,157,123},{ 36,88,162,123},{ 36,91,168,123},{ 36,92,173,123},{ 36,95,179,123},{ 36,97,184,123},{ 36,100,188,123},
		{ 36,78,107,125},{ 36,78,109,124},{ 24,79,112,124, 12,80,112,124},{ 24,81,116,123, 12,82,116,123},{ 24,83,120,123, 12,82,120,123},{ 36,84,125,122},{ 36,86,131,122},{ 36,88,136,122},{ 36,89,141,122},{ 36,91,147,122},{ 36,94,152,121},{ 24,95,158,121, 12,96,158,121},{ 36,98,163,121},{ 36,100,169,121},{ 36,103,174,121},{ 36,104,180,121},{ 36,107,185,121},
		{ 36,87,104,122},{ 36,87,106,122},{ 36,88,108,122},{ 36,90,112,121},{ 36,91,117,121},{ 36,93,121,120},{ 36,94,127,120},{ 36,96,132,120},{ 36,97,137,119},{ 36,99,142,120},{ 36,102,148,119},{ 36,104,153,119},{ 36,106,159,119},{ 36,108,164,119},{ 36,110,170,119},{ 36,112,175,118},{ 36,115,181,118},
		{ 36,95,102,120},{ 36,96,103,120},{ 36,97,105,120},{ 36,98,109,119},{ 36,99,113,119},{ 36,101,118,119},{ 36,102,122,118},{ 36,105,127,118},{ 36,107,133,118},{ 36,108,139,117},{ 36,110,144,117},{ 36,112,150,117},{ 36,114,155,117},{ 36,116,161,117},{ 36,118,166,116},{ 36,120,172,117},{ 36,123,176,116},
		{ 36,104,99,118},{ 36,105,100,118},{ 36,106,102,118},{ 36,107,106,118},{ 36,108,110,117},{ 36,110,114,117},{ 36,111,119,116},{ 36,113,124,116},{ 36,115,129,116},{ 36,116,134,115},{ 24,119,140,115, 12,118,140,115},{ 36,120,145,115},{ 36,122,151,115},{ 36,124,156,115},{ 36,126,162,115},{ 36,129,167,115},{ 36,131,173,114},
		{ 36,113,97,117},{ 36,114,97,116},{ 36,114,100,116},{ 36,116,102,116},{ 36,117,106,115},{ 36,118,110,115},{ 36,119,115,115},{ 28,121,120,115, 8,122,120,115},{ 36,124,125,114},{ 36,125,130,114},{ 36,127,136,114},{ 36,128,141,114},{ 36,131,146,113},{ 36,132,152,113},{ 36,135,157,113},{ 24,138,162,113, 12,137,162,113},{ 36,139,168,113},
		{ 36,122,94,115},{ 36,123,95,115},{ 36,123,96,115},{ 36,125,99,114},{ 36,125,103,114},{ 36,127,107,114},{ 36,129,111,113},{ 32,131,116,113, 4,130,116,113},{ 36,132,122,112},{ 36,134,127,112},{ 36,136,132,112},{ 36,137,137,112},{ 36,139,143,112},{ 36,141,148,111},{ 36,143,153,111},{ 36,146,159,111},{ 36,147,163,111},
		{ 36,131,91,113},{ 36,132,92,113},{ 36,132,94,113},{ 36,134,96,112},{ 36,135,99,112},{ 36,136,103,112},{ 36,138,108,112},{ 36,139,112,111},{ 36,141,117,111},{ 32,143,122,111, 4,142,122,111},{ 36,144,127,110},{ 36,146,133,110},{ 36,148,138,110},{ 36,150,143,110},{ 36,151,149,110},{ 36,154,154,109},{ 32,156,160,109, 4,155,160,109},
		{ 36,140,89,112},{ 36,141,89,111},{ 36,141,91,111},{ 36,143,93,111},{ 36,144,96,111},{ 36,145,100,110},{ 36,147,104,110},{ 36,148,109,110},{ 36,150,113,109},{ 36,151,118,109},{ 36,153,123,109},{ 36,155,128,109},{ 36,156,134,108},{ 36,158,139,108},{ 36,160,144,108},{ 36,163,150,108},{ 36,164,155,108},
		{ 36,149,86,110},{ 36,150,86,110},{ 36,150,88,110},{ 36,152,90,109},{ 36,153,93,109},{ 36,154,97,109},{ 36,156,101,109},{ 36,157,105,108},{ 36,158,110,108},{ 36,160,115,108},{ 36,162,120,107},{ 36,164,125,107},{ 36,165,130,107},{ 36,167,135,107},{ 36,169,141,106},{ 36,171,146,106},{ 36,172,150,106},
		{ 36,158,83,109},{ 36,159,84,109},{ 36,160,85,108},{ 36,161,87,108},{ 36,162,90,108},{ 36,163,93,108},{ 36,165,97,107},{ 36,166,101,107},{ 36,167,106,107},{ 36,169,111,106},{ 36,171,115,106},{ 36,173,121,106},{ 36,174,126,105},{ 36,176,131,105},{ 36,177,136,105},{ 36,179,141,105},{ 36,181,147,105},
		{ 36,167,81,108},{ 36,168,81,107},{ 36,168,82,107},{ 36,170,84,107},{ 36,170,87,107},{ 36,172,90,106},{ 36,173,94,106},{ 36,175,98,106},{ 36,176,102,105},{ 36,177,107,105},{ 36,179,112,105},{ 36,180,117,105},{ 36,182,122,104},{ 36,184,127,104},{ 36,186,132,104},{ 36,187,138,104},{ 36,189,142,103},
		{ 36,40,120,144},{ 36,41,123,143},{ 36,42,127,143},{ 36,43,132,143},{ 36,45,138,142},{ 36,46,144,141},{ 36,48,149,141},{ 36,51,155,140},{ 36,53,161,139},{ 36,55,166,139},{ 36,57,172,138},{ 36,60,177,138},{ 36,62,182,137},{ 36,65,187,137},{ 36,68,193,136},{ 36,72,198,136},{ 36,75,203,136},
		{ 36,45,118,141},{ 36,45,121,141},{ 36,47,125,141},{ 36,48,130,140},{ 36,50,136,139},{ 36,51,142,139},{ 36,53,147,138},{ 36,56,153,137},{ 36,57,158,137},{ 36,60,164,136},{ 36,62,170,136},{ 36,65,175,136},{ 36,67,180,136},{ 36,70,185,135},{ 36,73,191,134},{ 36,75,196,134},{ 36,78,200,134},
		{ 36,51,115,139},{ 36,51,118,138},{ 36,52,122,138},{ 36,53,128,137},{ 36,56,133,136},{ 36,58,138,136},{ 36,59,144,135},{ 36,61,150,135},{ 36,63,155,134},{ 36,65,160,134},{ 36,67,166,134},{ 36,70,172,133},{ 36,72,177,133},{ 36,75,182,133},{ 36,78,187,132},{ 36,80,193,132},{ 36,83,198,132},
		{ 36,58,113,135},{ 36,58,115,135},{ 36,59,119,134},{ 36,60,124,134},{ 36,62,129,133},{ 36,64,135,133},{ 36,66,140,133},{ 36,68,146,132},{ 36,69,151,131},{ 36,72,157,131},{ 24,73,162,131, 12,74,162,131},{ 36,76,168,130},{ 36,79,174,130},{ 36,81,179,130},{ 36,84,185,130},{ 36,86,190,129},{ 36,89,194,129},
		{ 36,65,111,133},{ 36,65,113,132},{ 36,67,117,131},{ 36,69,121,131},{ 36,70,126,131},{ 36,72,132,130},{ 36,73,137,130},{ 36,75,142,129},{ 36,77,148,128},{ 36,79,153,128},{ 36,80,159,128},{ 36,83,165,128},{ 24,85,170,128, 12,86,170,128},{ 36,88,175,128},{ 36,91,181,128},{ 24,93,186,128, 12,92,186,128},{ 36,96,192,127},
		{ 36,73,108,130},{ 36,73,110,130},{ 36,75,113,129},{ 36,76,118,128},{ 36,77,122,128},{ 36,80,128,128},{ 36,80,133,127},{ 36,82,138,127},{ 36,84,144,127},{ 36,87,149,126},{ 36,89,155,126},{ 24,90,160,126, 12,91,160,126},{ 36,93,166,126},{ 36,95,171,125},{ 36,97,177,125},{ 36,99,182,125},{ 36,102,187,125},
		{ 36,81,106,127},{ 36,82,107,127},{ 36,83,110,127},{ 36,85,114,126},{ 36,85,119,125},{ 36,87,124,125},{ 36,88,129,125},{ 36,90,134,124},{ 36,92,140,124},{ 36,94,145,124},{ 36,96,150,123},{ 36,98,157,123},{ 36,100,162,123},{ 36,102,168,123},{ 36,105,173,123},{ 36,107,179,123},{ 24,109,183,122, 12,110,183,122},
		{ 36,89,103,125},{ 36,90,105,125},{ 36,91,107,124},{ 24,92,111,124, 12,93,111,124},{ 36,93,115,123},{ 36,95,120,123},{ 36,96,125,122},{ 36,99,131,122},{ 36,101,136,122},{ 36,102,142,122},{ 36,104,147,122},{ 36,106,152,121},{ 36,108,158,121},{ 36,110,163,121},{ 36,112,169,120},{ 36,115,174,120},{ 36,117,180,120},
		{ 36,98,101,123},{ 36,99,102,122},{ 36,99,104,122},{ 36,101,108,122},{ 36,102,112,121},{ 36,104,116,121},{ 36,105,121,120},{ 36,107,127,120},{ 36,109,132,120},{ 36,110,137,119},{ 36,112,143,119},{ 36,114,148,119},{ 36,116,153,119},{ 36,118,159,118},{ 36,120,164,118},{ 36,123,170,118},{ 36,124,175,118},
		{ 20,107,98,120, 16,106,98,120},{ 36,108,99,120},{ 36,108,101,120},{ 36,110,104,120},{ 36,110,108,119},{ 28,112,113,119, 8,113,113,119},{ 36,114,117,118},{ 36,115,122,118},{ 36,117,127,117},{ 28,118,134,117, 8,119,134,117},{ 36,121,139,117},{ 36,122,144,117},{ 36,125,150,116},{ 36,127,155,116},{ 36,129,161,116},{ 36,131,166,116},{ 36,133,171,116},
		{ 36,115,96,119},{ 36,116,97,118},{ 36,117,99,118},{ 36,118,102,118},{ 36,119,105,117},{ 28,120,110,117, 8,121,110,117},{ 36,123,114,116},{ 36,124,119,116},{ 36,126,124,116},{ 36,127,129,115},{ 36,129,135,115},{ 20,130,140,115, 16,131,140,115},{ 28,132,146,115, 8,133,146,115},{ 36,135,151,114},{ 36,137,156,114},{ 36,139,162,114},{ 36,141,167,114},
		{ 36,124,93,117},{ 36,125,94,116},{ 36,126,96,116},{ 36,127,98,116},{ 36,129,102,115},{ 36,129,106,115},{ 36,131,110,115},{ 36,132,115,114},{ 36,134,120,114},{ 36,136,125,114},{ 36,138,130,113},{ 36,140,136,113},{ 36,141,141,113},{ 36,143,146,113},{ 36,145,152,112},{ 36,147,157,112},{ 36,149,163,112},
		{ 36,133,91,115},{ 36,134,91,115},{ 36,134,93,114},{ 36,136,95,114},{ 36,137,98,114},{ 36,138,102,113},{ 36,140,107,113},{ 36,141,111,113},{ 36,143,117,112},{ 36,144,122,112},{ 36,146,127,111},{ 36,148,132,111},{ 36,149,137,111},{ 36,152,143,111},{ 36,153,148,111},{ 36,156,153,110},{ 36,157,158,110},
		{ 36,142,88,113},{ 36,143,89,113},{ 36,144,90,113},{ 36,145,92,112},{ 36,146,95,112},{ 36,147,99,112},{ 36,149,103,111},{ 32,150,108,111, 4,149,108,111},{ 36,152,113,111},{ 24,153,117,111, 12,154,117,111},{ 36,155,123,110},{ 36,157,127,110},{ 36,158,133,110},{ 36,160,138,109},{ 24,162,144,110, 12,161,144,110},{ 36,164,149,109},{ 24,166,154,109, 12,165,154,109},
		{ 36,151,86,111},{ 36,152,86,111},{ 36,153,87,111},{ 36,154,89,111},{ 36,155,92,111},{ 36,156,96,110},{ 36,158,100,110},{ 36,158,104,109},{ 36,160,108,109},{ 36,162,113,109},{ 36,163,118,109},{ 36,166,124,108},{ 36,167,129,108},{ 36,169,134,108},{ 36,170,139,108},{ 36,172,145,107},{ 36,174,150,107},
		{ 36,161,82,110},{ 36,161,83,110},{ 36,162,84,110},{ 36,163,86,109},{ 36,164,89,109},{ 24,164,92,109, 12,165,92,109},{ 36,166,96,108},{ 36,168,101,108},{ 36,169,105,108},{ 36,171,110,108},{ 36,172,115,107},{ 36,174,120,107},{ 36,175,125,106},{ 36,178,130,106},{ 36,179,136,106},{ 36,181,141,106},{ 36,183,146,106},
		{ 36,169,80,108},{ 36,170,80,108},{ 36,170,82,108},{ 36,171,83,108},{ 36,172,86,108},{ 36,173,89,108},{ 36,174,93,107},{ 36,176,97,107},{ 36,178,101,106},{ 36,179,106,106},{ 36,181,111,106},{ 36,181,116,106},{ 36,184,121,105},{ 36,185,126,105},{ 36,187,131,105},{ 36,190,136,104},{ 36,191,141,104},
		{ 36,45,118,147},{ 36,46,121,147},{ 36,47,125,146},{ 36,48,130,146},{ 36,50,136,145},{ 36,51,142,144},{ 36,53,147,143},{ 36,56,153,143},{ 36,57,158,143},{ 36,59,164,142},{ 36,61,170,142},{ 36,64,175,141},{ 36,66,180,140},{ 36,69,186,140},{ 36,72,191,139},{ 36,75,196,139},{ 36,78,201,138},
		{ 36,50,116,145},{ 36,50,119,144},{ 36,52,123,144},{ 36,52,128,143},{ 36,54,133,143},{ 36,56,139,142},{ 36,57,145,141},{ 36,60,150,140},{ 36,61,156,140},{ 36,64,161,140},{ 36,65,167,139},{ 36,68,173,139},{ 36,71,178,138},{ 36,73,183,138},{ 36,76,188,137},{ 20,78,194,137, 16,79,194,137},{ 36,82,199,136},
		{ 36,55,114,142},{ 36,56,116,141},{ 36,57,121,141},{ 36,58,126,140},{ 36,60,131,140},{ 36,62,137,139},{ 36,63,142,139},{ 36,65,148,138},{ 36,67,153,137},{ 36,69,159,137},{ 36,71,165,136},{ 36,74,170,136},{ 36,76,175,136},{ 36,78,180,135},{ 36,81,186,135},{ 36,83,191,134},{ 36,86,196,134},
		{ 36,62,112,139},{ 36,62,114,138},{ 36,64,118,138},{ 36,65,122,137},{ 36,66,128,137},{ 36,68,133,136},{ 36,69,138,136},{ 36,72,144,135},{ 36,73,149,134},{ 36,75,155,134},{ 36,78,160,134},{ 24,80,166,133, 12,79,166,133},{ 36,82,172,133},{ 36,84,177,133},{ 36,87,182,132},{ 36,89,188,132},{ 36,92,193,132},
		{ 36,69,110,136},{ 36,69,111,135},{ 36,71,115,135},{ 36,72,119,134},{ 36,73,124,134},{ 36,75,129,133},{ 36,76,135,133},{ 36,78,140,132},{ 36,80,145,132},{ 36,82,151,131},{ 36,85,157,131},{ 36,86,162,131},{ 36,89,168,130},{ 36,91,174,130},{ 36,93,179,130},{ 36,95,185,130},{ 36,98,189,129},
		{ 36,76,107,133},{ 36,78,109,133},{ 36,78,112,132},{ 36,80,116,132},{ 36,81,121,131},{ 36,83,126,130},{ 36,84,131,130},{ 36,86,137,130},{ 36,88,142,129},{ 36,90,148,129},{ 36,92,153,128},{ 36,93,159,128},{ 36,96,165,128},{ 36,97,170,128},{ 36,100,175,127},{ 36,102,181,127},{ 36,105,185,127},
		{ 36,84,105,130},{ 36,85,106,130},{ 36,86,109,130},{ 36,87,113,129},{ 36,88,117,129},{ 36,90,122,128},{ 36,92,128,128},{ 36,93,133,127},{ 36,96,138,127},{ 36,97,144,127},{ 36,99,149,126},{ 36,101,155,126},{ 36,103,160,126},{ 36,105,166,125},{ 36,107,171,125},{ 36,110,177,125},{ 36,112,182,125},
		{ 36,93,102,127},{ 36,93,103,127},{ 36,94,106,127},{ 36,96,110,127},{ 36,96,114,126},{ 36,98,119,125},{ 36,100,124,125},{ 36,101,129,125},{ 24,103,134,124, 12,104,134,124},{ 36,105,140,124},{ 36,107,145,124},{ 36,108,150,123},{ 36,111,157,123},{ 36,113,162,123},{ 36,115,168,123},{ 36,118,173,122},{ 36,119,178,122},
		{ 36,101,100,125},{ 36,102,101,125},{ 36,102,103,125},{ 36,104,107,124},{ 36,104,111,124},{ 36,106,115,123},{ 36,108,120,123},{ 36,109,125,122},{ 36,111,131,122},{ 36,113,136,122},{ 36,115,142,122},{ 36,116,147,121},{ 36,119,152,121},{ 36,121,158,120},{ 36,123,163,120},{ 36,125,169,120},{ 36,127,174,120},
		{ 36,109,97,123},{ 36,110,98,123},{ 36,111,100,122},{ 36,112,104,122},{ 36,114,107,122},{ 36,115,112,121},{ 36,117,116,121},{ 36,118,121,120},{ 36,120,127,120},{ 36,121,132,120},{ 36,123,137,119},{ 36,125,143,119},{ 36,127,148,119},{ 36,129,153,118},{ 36,131,159,118},{ 36,133,164,118},{ 36,135,170,118},
		{ 28,117,95,120, 8,118,95,120},{ 36,119,96,120},{ 36,119,97,120},{ 36,121,100,120},{ 36,122,104,119},{ 36,123,108,119},{ 36,125,113,119},{ 36,126,118,118},{ 36,128,122,118},{ 36,129,127,117},{ 36,131,134,117},{ 36,134,139,117},{ 36,135,145,117},{ 36,137,150,116},{ 36,139,155,116},{ 36,141,161,116},{ 36,143,166,116},
		{ 36,126,93,119},{ 36,128,93,118},{ 36,129,95,118},{ 36,129,97,118},{ 36,131,101,117},{ 36,132,105,117},{ 36,133,110,117},{ 20,134,114,116, 16,135,114,116},{ 36,137,119,116},{ 36,138,124,116},{ 36,140,129,115},{ 36,142,135,115},{ 36,143,140,115},{ 28,145,146,115, 8,146,146,115},{ 36,147,151,114},{ 36,149,156,114},{ 36,151,162,114},
		{ 36,135,90,117},{ 36,136,90,116},{ 36,137,92,116},{ 36,138,94,116},{ 20,139,98,116, 16,140,98,116},{ 36,140,102,115},{ 36,142,106,115},{ 36,143,110,115},{ 36,145,115,114},{ 36,147,120,114},{ 36,148,125,113},{ 36,150,130,113},{ 36,152,136,113},{ 36,154,141,113},{ 36,155,147,112},{ 36,158,152,112},{ 36,159,157,112},
		{ 36,145,87,115},{ 36,145,88,115},{ 36,146,89,114},{ 36,147,91,114},{ 36,148,94,114},{ 36,149,98,113},{ 36,151,102,113},{ 36,152,106,113},{ 36,154,111,112},{ 36,156,117,112},{ 36,157,122,111},{ 36,159,127,111},{ 36,160,132,111},{ 36,162,137,111},{ 36,164,143,111},{ 36,166,148,110},{ 36,167,153,110},
		{ 24,153,85,113, 12,154,85,113},{ 36,154,85,113},{ 36,155,86,113},{ 36,156,89,112},{ 36,157,91,112},{ 36,158,95,112},{ 28,159,99,111, 8,160,99,111},{ 36,161,103,111},{ 36,162,108,111},{ 36,164,112,110},{ 36,165,117,110},{ 36,168,123,109},{ 36,169,127,109},{ 36,171,133,109},{ 36,173,138,109},{ 36,174,144,109},{ 36,176,149,108},
		{ 36,163,82,111},{ 36,163,82,111},{ 36,164,84,111},{ 36,164,85,111},{ 36,166,88,110},{ 36,167,91,110},{ 36,168,95,110},{ 36,170,99,109},{ 36,171,104,109},{ 36,173,109,109},{ 36,174,113,108},{ 36,176,118,108},{ 36,177,124,108},{ 36,179,129,108},{ 36,181,134,107},{ 36,183,139,107},{ 36,185,145,107},
		{ 36,171,79,110},{ 36,172,80,110},{ 36,172,81,110},{ 36,173,83,109},{ 36,174,85,109},{ 36,175,88,109},{ 36,177,92,108},{ 36,178,96,108},{ 32,180,101,107, 4,179,101,107},{ 24,180,105,107, 12,181,105,107},{ 36,182,110,107},{ 36,183,115,107},{ 36,186,120,106},{ 36,188,125,106},{ 36,189,130,106},{ 36,191,136,106},{ 36,193,141,105},
		{ 36,51,116,150},{ 36,51,118,150},{ 36,53,123,150},{ 36,53,127,149},{ 36,55,133,148},{ 36,57,139,148},{ 36,58,145,147},{ 36,60,151,146},{ 36,62,156,145},{ 36,64,162,145},{ 36,66,168,144},{ 36,68,173,144},{ 36,71,179,144},{ 36,73,184,143},{ 36,76,189,142},{ 36,78,194,142},{ 36,81,199,141},
		{ 36,55,115,148},{ 36,55,117,148},{ 36,57,121,147},{ 36,58,126,147},{ 36,59,131,146},{ 36,61,137,145},{ 36,62,142,144},{ 36,64,148,144},{ 36,66,154,143},{ 36,68,159,143},{ 36,70,165,142},{ 36,72,170,142},{ 36,75,176,141},{ 36,77,181,141},{ 36,80,187,140},{ 36,82,192,139},{ 36,85,197,139},
		{ 36,60,113,145},{ 36,60,115,145},{ 36,62,118,144},{ 36,63,123,144},{ 36,64,128,143},{ 36,66,134,143},{ 36,67,139,142},{ 36,70,145,141},{ 36,71,150,140},{ 36,73,156,140},{ 36,76,162,140},{ 36,77,167,139},{ 36,80,173,139},{ 36,82,178,138},{ 24,84,183,138, 12,85,183,138},{ 36,87,189,137},{ 36,90,194,137},
		{ 36,66,110,143},{ 36,67,112,142},{ 36,68,116,141},{ 36,70,121,141},{ 36,70,126,140},{ 36,72,131,140},{ 36,74,137,139},{ 36,76,142,138},{ 36,77,148,138},{ 36,79,153,137},{ 36,82,159,137},{ 36,83,165,136},{ 36,86,170,136},{ 36,88,175,136},{ 36,90,181,135},{ 36,92,186,134},{ 36,95,191,134},
		{ 36,73,108,139},{ 36,74,110,139},{ 36,75,113,138},{ 36,76,118,138},{ 36,77,123,137},{ 36,79,128,137},{ 36,80,133,136},{ 36,82,138,136},{ 36,84,144,135},{ 36,86,150,134},{ 36,88,155,134},{ 36,90,161,134},{ 36,92,166,133},{ 36,94,172,133},{ 36,97,177,133},{ 36,99,183,132},{ 36,101,188,132},
		{ 36,80,106,136},{ 36,81,108,136},{ 36,82,110,135},{ 36,84,114,135},{ 36,84,119,134},{ 36,86,124,134},{ 36,87,129,133},{ 36,89,135,133},{ 36,91,140,132},{ 36,93,146,132},{ 36,95,151,131},{ 36,96,157,131},{ 36,99,162,131},{ 24,101,168,130, 12,100,168,130},{ 36,103,174,130},{ 36,106,179,130},{ 36,108,184,130},
		{ 36,88,104,133},{ 36,89,105,133},{ 36,89,108,133},{ 36,91,112,132},{ 36,92,116,132},{ 36,94,121,131},{ 36,95,126,130},{ 36,97,131,130},{ 36,99,137,130},{ 36,100,142,129},{ 36,102,148,128},{ 36,104,153,128},{ 36,106,159,128},{ 36,109,165,128},{ 36,110,170,128},{ 36,113,175,127},{ 24,114,180,127, 12,115,180,127},
		{ 36,96,101,131},{ 36,96,103,130},{ 36,97,105,130},{ 36,99,108,130},{ 24,99,113,129, 12,100,113,129},{ 36,101,117,129},{ 36,103,122,128},{ 36,105,128,128},{ 36,106,133,127},{ 36,108,138,127},{ 36,110,144,127},{ 36,111,149,126},{ 36,114,155,126},{ 36,116,160,125},{ 24,117,166,125, 12,118,166,125},{ 36,120,171,125},{ 36,122,177,125},
		{ 36,104,99,128},{ 36,105,100,127},{ 36,105,102,127},{ 36,107,105,127},{ 36,108,109,127},{ 36,109,113,126},{ 36,111,119,125},{ 36,112,124,125},{ 36,114,129,125},{ 36,115,134,124},{ 36,118,140,124},{ 36,120,145,124},{ 36,122,150,123},{ 36,124,157,123},{ 36,126,162,123},{ 36,128,168,122},{ 36,130,172,122},
		{ 36,112,97,125},{ 36,113,97,125},{ 36,114,100,125},{ 36,115,103,125},{ 24,116,106,124, 12,117,106,124},{ 36,117,111,124},{ 36,119,115,123},{ 36,121,120,123},{ 20,122,125,122, 16,123,125,122},{ 36,124,131,122},{ 36,126,136,122},{ 36,128,142,122},{ 36,129,147,121},{ 36,132,152,120},{ 36,133,158,120},{ 36,136,163,120},{ 36,137,169,120},
		{ 36,120,94,123},{ 36,121,95,123},{ 36,123,96,123},{ 36,124,100,122},{ 36,125,103,122},{ 36,126,107,122},{ 36,128,112,121},{ 36,129,117,120},{ 36,131,121,120},{ 36,133,127,120},{ 36,134,132,119},{ 36,136,137,119},{ 36,137,143,119},{ 36,140,148,119},{ 36,141,153,118},{ 36,144,159,118},{ 36,145,165,117},
		{ 36,129,92,121},{ 36,130,92,121},{ 36,131,94,121},{ 36,132,96,120},{ 36,134,100,120},{ 36,134,104,119},{ 36,136,108,119},{ 36,137,113,118},{ 36,139,118,118},{ 36,141,122,118},{ 36,142,127,117},{ 36,144,134,117},{ 36,145,139,117},{ 36,148,144,117},{ 36,149,150,116},{ 24,152,155,116, 12,151,155,116},{ 36,153,160,115},
		{ 36,138,89,119},{ 36,139,90,119},{ 36,140,91,118},{ 36,140,93,118},{ 36,142,97,118},{ 36,143,101,117},{ 36,144,105,117},{ 36,146,110,117},{ 36,148,114,116},{ 20,149,119,115, 16,150,119,115},{ 36,151,124,115},{ 36,153,129,115},{ 36,154,135,115},{ 36,156,140,115},{ 36,157,146,114},{ 36,160,151,114},{ 36,162,156,114},
		{ 36,147,87,117},{ 36,147,87,117},{ 36,149,88,116},{ 36,149,90,116},{ 24,150,93,116, 12,151,93,116},{ 36,151,97,116},{ 36,153,101,115},{ 36,155,106,115},{ 36,156,110,114},{ 36,158,115,114},{ 36,159,120,113},{ 36,161,125,113},{ 36,162,130,113},{ 36,164,136,113},{ 24,167,141,112, 12,166,141,112},{ 36,168,147,112},{ 36,170,152,112},
		{ 36,156,84,115},{ 36,156,84,115},{ 36,158,85,114},{ 36,158,87,114},{ 36,159,90,114},{ 36,161,94,113},{ 36,161,98,113},{ 36,163,102,113},{ 36,164,106,113},{ 36,166,111,112},{ 36,168,117,111},{ 36,170,122,111},{ 36,171,127,111},{ 36,173,132,111},{ 36,175,137,111},{ 36,176,143,110},{ 36,178,147,110},
		{ 28,164,81,113, 8,165,81,113},{ 36,165,82,113},{ 36,166,83,113},{ 36,167,85,112},{ 36,168,87,112},{ 36,170,91,112},{ 36,170,94,111},{ 36,172,99,111},{ 36,173,103,111},{ 36,175,108,110},{ 36,176,113,110},{ 36,178,118,109},{ 36,180,123,109},{ 36,181,128,109},{ 36,183,133,109},{ 36,185,138,108},{ 36,187,144,108},
		{ 36,173,79,111},{ 36,174,79,111},{ 36,174,80,111},{ 36,175,82,111},{ 36,176,84,111},{ 36,177,87,110},{ 36,179,91,110},{ 36,180,95,109},{ 24,182,99,109, 12,181,99,109},{ 32,183,104,109, 4,182,104,109},{ 36,184,110,109},{ 36,186,114,108},{ 36,187,119,108},{ 36,190,124,108},{ 36,191,129,107},{ 36,193,135,107},{ 24,194,139,107, 12,195,139,107},
		{ 36,56,114,154},{ 24,56,116,153, 12,57,116,153},{ 36,58,121,153},{ 36,59,125,152},{ 36,60,130,152},{ 36,62,136,151},{ 36,63,142,150},{ 36,65,147,149},{ 36,66,153,149},{ 36,68,159,148},{ 36,71,165,148},{ 36,73,170,147},{ 36,75,176,147},{ 36,77,181,146},{ 36,80,186,145},{ 36,82,192,145},{ 36,85,197,144},
		{ 36,60,113,151},{ 36,61,115,151},{ 36,62,118,150},{ 36,63,123,150},{ 36,64,128,150},{ 36,66,134,149},{ 36,67,140,148},{ 36,69,146,147},{ 36,70,151,147},{ 28,72,157,146, 8,73,157,146},{ 36,75,163,145},{ 36,77,168,145},{ 36,79,174,144},{ 36,81,179,144},{ 36,84,185,143},{ 36,86,190,142},{ 36,88,194,142},
		{ 36,65,111,148},{ 36,66,113,148},{ 36,67,117,148},{ 36,68,121,147},{ 36,69,126,147},{ 36,71,132,146},{ 36,72,137,145},{ 36,74,143,145},{ 36,76,148,144},{ 36,77,154,143},{ 36,80,160,143},{ 36,81,165,142},{ 36,84,171,142},{ 36,86,176,141},{ 36,89,182,141},{ 36,91,187,140},{ 36,93,192,140},
		{ 36,71,109,146},{ 36,72,111,145},{ 36,72,114,145},{ 36,74,118,144},{ 36,75,123,143},{ 36,77,128,143},{ 36,78,134,142},{ 36,80,140,142},{ 36,82,145,141},{ 36,83,151,140},{ 32,85,156,140, 4,86,156,140},{ 36,87,162,139},{ 36,89,167,139},{ 36,91,173,139},{ 36,94,178,138},{ 24,96,184,137, 12,97,184,137},{ 24,98,189,137, 12,99,189,137},
		{ 36,77,107,143},{ 36,78,108,142},{ 36,79,111,142},{ 36,80,116,141},{ 36,81,121,140},{ 36,83,126,140},{ 36,85,131,140},{ 36,86,137,139},{ 36,88,142,138},{ 36,90,148,138},{ 36,92,153,137},{ 36,93,159,137},{ 36,96,165,136},{ 36,98,170,136},{ 36,100,176,136},{ 36,103,181,135},{ 36,104,185,135},
		{ 36,84,105,140},{ 28,85,106,139, 8,86,106,139},{ 36,86,109,139},{ 36,87,113,138},{ 36,88,118,138},{ 36,90,123,137},{ 36,92,128,137},{ 36,93,133,136},{ 36,95,139,136},{ 36,96,144,135},{ 36,99,150,134},{ 36,100,155,134},{ 36,102,160,133},{ 36,105,166,133},{ 36,106,172,133},{ 36,109,177,132},{ 36,111,182,132},
		{ 36,91,103,137},{ 36,93,104,136},{ 36,93,106,136},{ 36,95,110,135},{ 36,96,114,135},{ 36,97,119,134},{ 36,99,124,134},{ 36,100,129,133},{ 36,102,135,133},{ 36,103,140,132},{ 36,105,146,132},{ 36,108,151,131},{ 36,109,157,131},{ 36,112,162,131},{ 36,113,168,130},{ 36,116,174,130},{ 24,117,179,130, 12,118,179,130},
		{ 36,99,100,134},{ 36,100,101,133},{ 36,101,103,133},{ 36,102,107,133},{ 36,104,111,132},{ 24,105,116,132, 12,104,116,132},{ 36,107,121,131},{ 36,108,126,130},{ 36,110,131,130},{ 36,111,137,130},{ 36,113,142,129},{ 36,115,148,128},{ 36,117,153,128},{ 36,119,159,128},{ 36,121,164,128},{ 36,123,170,128},{ 36,125,175,127},
		{ 36,107,98,131},{ 36,108,99,131},{ 36,109,101,130},{ 36,110,104,130},{ 36,112,108,130},{ 20,113,112,129, 16,112,112,129},{ 36,114,117,128},{ 36,115,122,128},{ 36,118,128,128},{ 36,120,133,127},{ 36,121,138,127},{ 36,123,144,127},{ 36,125,149,126},{ 36,127,155,125},{ 36,128,160,125},{ 36,130,166,125},{ 36,132,171,125},
		{ 36,115,95,128},{ 36,116,96,128},{ 36,118,98,128},{ 36,118,101,127},{ 36,120,105,127},{ 36,121,109,127},{ 36,123,113,126},{ 36,124,118,125},{ 36,126,123,125},{ 36,128,129,125},{ 36,129,134,124},{ 36,131,140,124},{ 36,132,145,124},{ 20,134,150,123, 16,135,150,123},{ 36,136,157,123},{ 28,138,162,122, 8,139,162,122},{ 36,140,167,122},
		{ 36,125,93,126},{ 36,125,94,126},{ 20,125,96,125, 16,126,96,125},{ 36,126,98,125},{ 36,128,102,125},{ 20,128,106,124, 16,129,106,124},{ 36,130,111,124},{ 36,132,115,123},{ 36,133,120,123},{ 36,136,125,122},{ 36,137,131,122},{ 36,139,136,122},{ 36,140,142,121},{ 36,142,147,121},{ 36,144,152,120},{ 36,146,158,120},{ 36,148,163,120},
		{ 36,133,91,123},{ 36,133,91,123},{ 36,134,93,123},{ 36,135,95,123},{ 36,136,99,122},{ 36,137,103,122},{ 36,139,107,122},{ 36,141,112,121},{ 36,142,117,121},{ 36,144,122,120},{ 36,145,127,120},{ 36,147,132,119},{ 36,148,137,119},{ 36,150,143,119},{ 36,152,148,118},{ 36,154,153,118},{ 36,156,159,117},
		{ 36,141,88,121},{ 36,142,89,121},{ 36,143,90,121},{ 36,143,92,120},{ 36,145,95,120},{ 36,145,99,120},{ 36,147,103,119},{ 36,149,108,119},{ 36,150,113,118},{ 36,152,118,118},{ 36,153,122,118},{ 36,155,127,117},{ 36,156,134,117},{ 28,158,139,117, 8,159,139,117},{ 36,160,144,116},{ 36,162,150,116},{ 36,164,154,116},
		{ 36,150,86,119},{ 36,150,86,119},{ 36,151,87,119},{ 36,151,90,118},{ 36,153,93,118},{ 36,155,96,117},{ 20,156,100,117, 16,155,100,117},{ 36,157,105,117},{ 36,158,109,117},{ 36,160,114,116},{ 36,161,119,115},{ 36,163,125,115},{ 36,166,129,115},{ 36,167,135,115},{ 36,169,140,114},{ 36,170,146,114},{ 36,172,151,114},
		{ 36,158,83,117},{ 36,158,84,117},{ 36,160,85,117},{ 36,160,87,116},{ 20,162,89,116, 16,161,89,116},{ 36,163,93,116},{ 36,164,97,115},{ 36,166,101,115},{ 36,167,106,115},{ 36,168,110,114},{ 36,170,115,114},{ 36,172,120,113},{ 36,174,125,113},{ 36,175,130,113},{ 36,177,136,113},{ 36,178,141,112},{ 36,181,147,112},
		{ 36,167,80,115},{ 36,167,81,115},{ 36,168,82,115},{ 36,170,83,115},{ 36,170,86,114},{ 36,172,89,114},{ 36,172,93,113},{ 36,174,97,113},{ 36,175,102,113},{ 36,177,106,112},{ 36,179,111,112},{ 36,180,117,111},{ 36,182,122,111},{ 36,183,127,111},{ 36,185,132,111},{ 36,187,138,110},{ 36,189,142,110},
		{ 36,175,78,113},{ 36,176,78,113},{ 36,177,79,113},{ 36,177,81,113},{ 24,178,84,113, 12,179,84,113},{ 36,179,87,112},{ 36,181,90,112},{ 36,182,94,111},{ 36,184,98,111},{ 36,186,103,111},{ 36,186,108,110},{ 28,189,113,110, 8,188,113,110},{ 36,190,118,109},{ 36,192,123,110},{ 36,193,128,109},{ 36,195,133,109},{ 36,196,139,108},
		{ 36,62,113,157},{ 36,63,115,156},{ 36,64,118,156},{ 36,65,123,156},{ 24,65,128,155, 12,66,128,155},{ 36,67,134,154},{ 36,68,140,153},{ 36,70,145,153},{ 36,72,151,152},{ 36,74,157,152},{ 36,76,162,151},{ 36,77,168,150},{ 36,80,174,150},{ 36,81,179,149},{ 36,84,184,148},{ 36,86,190,148},{ 36,89,195,147},
		{ 36,65,111,154},{ 36,66,113,154},{ 36,67,117,154},{ 36,68,121,153},{ 36,69,126,153},{ 36,71,132,152},{ 36,72,137,152},{ 36,74,143,150},{ 36,76,148,150},{ 36,77,154,149},{ 36,79,160,149},{ 36,81,165,148},{ 36,83,171,148},{ 36,85,176,147},{ 36,88,182,146},{ 36,90,188,145},{ 24,92,193,145, 12,93,193,145},
		{ 36,70,110,152},{ 36,71,111,152},{ 36,71,114,151},{ 36,73,119,151},{ 36,74,124,150},{ 20,76,129,149, 16,75,129,149},{ 24,78,134,149, 12,77,134,149},{ 36,79,141,148},{ 36,81,146,147},{ 36,82,152,147},{ 36,84,158,146},{ 36,86,163,146},{ 36,88,169,145},{ 36,90,174,145},{ 36,92,180,144},{ 36,95,185,143},{ 36,97,190,143},
		{ 36,76,108,149},{ 36,76,110,149},{ 36,77,113,148},{ 36,78,117,148},{ 36,79,122,147},{ 36,81,127,147},{ 36,83,132,146},{ 36,84,137,145},{ 36,86,143,144},{ 36,87,149,144},{ 36,90,154,143},{ 36,91,160,143},{ 36,93,166,142},{ 36,96,171,142},{ 36,98,177,141},{ 36,100,182,141},{ 36,102,187,140},
		{ 24,82,106,146, 12,81,106,146},{ 36,83,107,146},{ 36,83,110,145},{ 24,84,114,145, 12,85,114,145},{ 36,86,118,144},{ 36,87,124,143},{ 36,89,129,143},{ 36,90,134,142},{ 36,92,140,142},{ 36,93,145,141},{ 36,96,151,140},{ 36,97,156,140},{ 36,99,162,139},{ 36,102,167,139},{ 20,104,173,138, 16,103,173,138},{ 36,106,178,138},{ 36,108,184,138},
		{ 36,88,104,143},{ 36,89,105,143},{ 36,90,107,142},{ 36,91,111,141},{ 36,93,115,141},{ 36,94,121,140},{ 36,96,126,140},{ 36,97,131,140},{ 36,99,137,139},{ 36,100,143,138},{ 36,102,148,137},{ 36,104,153,137},{ 36,106,159,137},{ 36,108,165,136},{ 36,110,170,136},{ 36,112,176,136},{ 36,114,180,135},
		{ 24,96,101,140, 12,95,101,140},{ 36,96,103,140},{ 36,98,105,139},{ 36,98,109,139},{ 36,100,113,138},{ 36,101,118,138},{ 36,103,123,137},{ 36,104,128,136},{ 36,106,133,136},{ 36,107,139,136},{ 36,109,144,135},{ 36,111,149,134},{ 36,113,155,134},{ 36,115,160,133},{ 36,117,166,133},{ 36,119,172,133},{ 36,121,177,132},
		{ 36,103,99,137},{ 36,104,100,137},{ 36,105,102,136},{ 36,106,105,136},{ 36,107,110,135},{ 36,108,114,135},{ 36,110,119,134},{ 36,111,124,134},{ 36,113,129,133},{ 36,115,135,133},{ 36,116,140,132},{ 24,118,145,132, 12,119,145,132},{ 36,120,151,131},{ 36,122,157,131},{ 36,124,162,130},{ 36,126,168,130},{ 36,128,173,130},
		{ 36,111,97,134},{ 36,112,97,134},{ 36,113,99,134},{ 36,113,102,133},{ 36,115,107,133},{ 36,116,111,132},{ 36,118,116,131},{ 36,119,121,131},{ 36,121,126,130},{ 36,123,131,130},{ 36,124,137,130},{ 36,126,142,129},{ 36,128,148,128},{ 36,130,153,128},{ 36,131,159,128},{ 36,134,165,128},{ 36,135,169,127},
		{ 36,119,94,132},{ 36,119,95,131},{ 36,121,97,131},{ 36,121,100,130},{ 36,123,104,130},{ 36,124,108,130},{ 36,126,112,128},{ 36,127,117,128},{ 36,129,122,128},{ 36,131,128,128},{ 36,132,133,127},{ 36,134,138,127},{ 36,135,144,127},{ 36,137,149,126},{ 36,140,155,125},{ 36,141,160,125},{ 36,143,166,125},
		{ 36,128,92,129},{ 36,128,93,128},{ 36,129,94,128},{ 24,130,97,128, 12,129,97,128},{ 36,131,100,127},{ 36,132,104,127},{ 36,134,109,127},{ 36,136,113,126},{ 36,137,118,126},{ 24,138,124,125, 12,139,124,125},{ 36,140,129,125},{ 36,142,134,124},{ 36,143,140,124},{ 36,145,145,124},{ 36,147,150,123},{ 36,149,156,123},{ 36,151,161,122},
		{ 36,136,90,126},{ 36,136,90,126},{ 36,137,92,126},{ 36,138,94,125},{ 36,139,98,125},{ 36,141,101,125},{ 36,142,106,124},{ 36,143,111,124},{ 36,144,115,123},{ 36,147,120,123},{ 36,148,126,122},{ 36,150,131,122},{ 36,152,136,121},{ 36,153,142,121},{ 36,155,147,121},{ 36,157,152,120},{ 36,159,157,120},
		{ 36,144,87,124},{ 36,144,88,123},{ 36,145,89,123},{ 36,146,91,123},{ 36,148,94,123},{ 36,149,98,122},{ 36,150,102,122},{ 36,152,107,121},{ 36,153,111,121},{ 36,155,117,120},{ 36,156,122,120},{ 36,158,127,120},{ 36,160,132,119},{ 36,161,137,119},{ 36,163,143,119},{ 36,165,148,118},{ 36,167,153,118},
		{ 36,153,85,122},{ 36,153,85,121},{ 36,154,86,121},{ 36,155,88,121},{ 36,156,91,121},{ 36,157,95,120},{ 36,158,99,120},{ 36,160,103,119},{ 36,161,108,119},{ 36,163,112,118},{ 36,165,118,118},{ 36,166,122,118},{ 36,168,128,117},{ 36,169,134,117},{ 36,171,139,117},{ 32,173,144,116, 4,172,144,116},{ 36,175,149,116},
		{ 36,161,82,120},{ 36,161,83,119},{ 36,162,84,119},{ 36,164,86,119},{ 36,164,89,118},{ 36,166,92,118},{ 36,167,96,118},{ 36,168,100,117},{ 36,169,105,117},{ 36,171,109,116},{ 36,173,114,116},{ 36,174,119,115},{ 36,176,125,115},{ 24,177,129,115, 12,178,129,115},{ 36,179,135,115},{ 36,181,140,114},{ 36,183,145,114},
		{ 36,170,80,117},{ 28,170,80,117, 8,171,80,117},{ 36,171,81,117},{ 36,172,83,117},{ 36,173,85,116},{ 36,174,89,116},{ 36,175,92,115},{ 36,177,96,115},{ 36,178,101,115},{ 36,179,106,115},{ 36,181,110,114},{ 36,183,115,114},{ 36,184,120,113},{ 36,186,125,113},{ 36,188,131,113},{ 36,189,136,113},{ 36,191,141,112},
		{ 36,178,77,115},{ 36,178,78,115},{ 36,179,79,115},{ 36,180,80,115},{ 36,181,83,115},{ 36,182,86,114},{ 36,183,89,114},{ 36,184,93,113},{ 36,186,98,113},{ 36,188,102,113},{ 36,189,107,112},{ 36,191,112,112},{ 36,192,117,112},{ 36,194,122,111},{ 36,195,127,111},{ 36,197,133,110},{ 36,198,137,110},
		{ 36,68,111,160},{ 36,68,113,159},{ 36,69,116,159},{ 36,70,120,159},{ 36,71,126,158},{ 36,72,131,158},{ 36,73,136,157},{ 36,75,142,156},{ 36,77,148,155},{ 36,78,153,155},{ 36,81,160,154},{ 36,82,166,154},{ 36,85,171,153},{ 36,86,177,152},{ 36,89,182,151},{ 36,91,188,151},{ 36,93,192,150},
		{ 36,71,110,158},{ 36,72,112,157},{ 36,72,115,157},{ 36,73,119,156},{ 36,74,124,156},{ 36,76,130,155},{ 36,78,135,155},{ 36,79,141,154},{ 36,81,146,153},{ 36,82,152,153},{ 36,84,158,152},{ 36,86,163,151},{ 36,88,169,151},{ 36,91,175,150},{ 36,92,180,149},{ 36,95,185,149},{ 24,96,191,148, 12,97,191,148},
		{ 36,75,108,156},{ 36,76,110,155},{ 36,76,113,154},{ 36,78,117,154},{ 36,79,122,153},{ 36,80,127,153},{ 36,82,132,152},{ 36,83,138,152},{ 36,85,143,151},{ 36,86,149,150},{ 28,88,155,149, 8,89,155,149},{ 36,90,161,149},{ 24,92,166,148, 12,93,166,148},{ 36,95,172,148},{ 36,97,177,147},{ 36,99,183,146},{ 36,101,188,146},
		{ 36,80,106,153},{ 36,81,108,152},{ 36,82,110,152},{ 36,83,114,151},{ 36,85,119,150},{ 36,86,124,150},{ 36,87,129,149},{ 36,89,135,149},{ 36,91,141,148},{ 36,92,147,147},{ 36,94,153,146},{ 36,96,158,146},{ 36,98,164,145},{ 36,100,169,145},{ 36,102,175,144},{ 36,104,180,144},{ 36,106,185,143},
		{ 36,86,104,150},{ 36,87,106,149},{ 36,88,109,148},{ 36,89,112,148},{ 36,91,117,147},{ 36,92,122,147},{ 36,93,127,146},{ 24,94,132,146, 12,95,132,146},{ 36,96,138,145},{ 36,98,143,144},{ 36,100,149,144},{ 36,102,155,143},{ 36,103,160,143},{ 36,106,166,142},{ 24,108,171,142, 12,107,171,142},{ 36,110,177,141},{ 36,112,182,141},
		{ 36,93,102,147},{ 36,94,103,146},{ 36,95,106,146},{ 24,95,110,145, 12,96,110,145},{ 36,97,114,144},{ 36,98,119,144},{ 36,100,124,143},{ 36,101,129,143},{ 36,103,134,142},{ 36,105,140,142},{ 36,106,145,141},{ 36,108,151,140},{ 36,110,156,140},{ 36,112,162,139},{ 36,114,167,139},{ 36,116,173,138},{ 36,118,179,138},
		{ 36,100,100,143},{ 36,101,101,143},{ 36,102,103,143},{ 36,102,107,142},{ 36,104,111,141},{ 24,104,115,141, 12,105,115,141},{ 36,107,121,140},{ 36,107,126,140},{ 36,110,131,139},{ 20,112,137,139, 16,111,137,139},{ 36,113,143,138},{ 36,115,148,137},{ 36,116,153,137},{ 36,119,159,136},{ 36,120,165,136},{ 36,123,170,136},{ 36,124,175,135},
		{ 36,108,98,140},{ 36,108,99,140},{ 36,109,101,140},{ 32,109,104,139, 4,110,104,139},{ 36,111,108,139},{ 36,112,113,138},{ 36,114,118,137},{ 36,116,123,137},{ 36,117,128,136},{ 36,119,133,136},{ 36,120,139,135},{ 36,122,144,135},{ 36,124,149,134},{ 36,126,155,134},{ 36,127,160,133},{ 36,130,166,133},{ 36,132,172,133},
		{ 36,115,95,137},{ 36,115,97,137},{ 36,116,98,137},{ 36,117,101,136},{ 36,119,105,136},{ 36,120,109,135},{ 36,121,114,135},{ 36,124,119,134},{ 36,124,124,134},{ 36,126,129,133},{ 20,127,135,133, 16,128,135,133},{ 36,130,140,132},{ 36,131,145,132},{ 36,133,151,131},{ 36,135,157,131},{ 36,137,162,130},{ 36,139,167,130},
		{ 36,123,93,135},{ 36,123,94,134},{ 36,124,96,134},{ 36,125,98,134},{ 36,126,102,133},{ 36,128,106,133},{ 36,129,111,132},{ 36,131,116,131},{ 36,132,121,131},{ 36,134,126,130},{ 36,135,132,130},{ 36,137,137,130},{ 36,138,142,129},{ 36,141,147,128},{ 36,143,153,128},{ 36,144,159,128},{ 36,146,163,128},
		{ 36,131,91,132},{ 36,131,92,132},{ 36,132,93,131},{ 36,133,96,131},{ 36,134,100,130},{ 36,136,103,130},{ 36,137,108,130},{ 36,139,112,128},{ 36,140,117,128},{ 36,142,122,128},{ 36,143,128,128},{ 36,145,133,127},{ 36,147,138,127},{ 36,148,144,127},{ 36,151,149,125},{ 36,152,155,125},{ 36,154,160,125},
		{ 36,139,89,129},{ 36,139,89,129},{ 36,140,91,128},{ 36,142,93,128},{ 36,142,96,128},{ 36,144,100,127},{ 36,145,104,127},{ 36,147,109,127},{ 36,148,113,126},{ 36,149,118,125},{ 36,150,124,125},{ 36,153,129,125},{ 36,155,134,124},{ 36,156,140,124},{ 36,158,145,124},{ 36,159,150,123},{ 36,162,156,123},
		{ 36,147,86,127},{ 36,147,87,126},{ 36,149,88,126},{ 36,150,90,126},{ 36,150,93,125},{ 36,152,97,125},{ 36,153,101,125},{ 36,155,106,124},{ 36,155,111,124},{ 36,157,115,123},{ 36,159,120,123},{ 36,160,125,122},{ 36,162,131,122},{ 36,164,136,121},{ 36,166,142,121},{ 36,167,147,121},{ 20,170,152,120, 16,169,152,120},
		{ 36,155,84,125},{ 36,157,84,124},{ 36,157,85,124},{ 36,158,87,123},{ 24,158,90,123, 12,159,90,123},{ 36,160,94,123},{ 36,161,98,122},{ 36,163,102,122},{ 36,164,107,121},{ 36,166,111,121},{ 36,168,117,120},{ 36,169,122,120},{ 36,171,127,120},{ 36,171,132,119},{ 36,174,137,119},{ 36,175,143,119},{ 36,177,148,118},
		{ 36,164,81,122},{ 36,165,82,122},{ 36,165,83,122},{ 36,166,85,121},{ 36,167,87,121},{ 36,169,90,120},{ 36,169,94,120},{ 36,171,99,120},{ 36,173,103,119},{ 36,174,107,119},{ 36,176,113,118},{ 36,177,118,118},{ 36,179,122,117},{ 36,180,128,117},{ 36,182,134,117},{ 36,184,139,116},{ 36,185,144,116},
		{ 36,172,79,120},{ 36,173,79,120},{ 36,173,80,119},{ 36,175,82,119},{ 36,175,85,119},{ 36,177,88,118},{ 28,177,91,118, 8,178,91,118},{ 36,179,95,118},{ 36,181,100,117},{ 36,182,105,117},{ 36,184,109,117},{ 36,185,114,116},{ 36,187,119,115},{ 36,188,125,115},{ 36,190,129,115},{ 36,192,135,114},{ 36,194,140,114},
		{ 36,181,76,118},{ 36,181,77,118},{ 36,182,77,117},{ 36,182,79,117},{ 36,183,81,117},{ 36,184,85,117},{ 36,186,88,116},{ 36,188,92,116},{ 36,188,96,115},{ 36,190,100,115},{ 36,191,105,115},{ 36,193,110,114},{ 36,194,115,114},{ 36,196,120,113},{ 36,198,126,113},{ 36,199,132,113},{ 36,202,136,112},
		{ 36,73,109,163},{ 36,74,111,163},{ 36,74,114,162},{ 36,76,119,162},{ 36,76,124,161},{ 36,78,129,161},{ 36,79,134,160},{ 36,80,140,159},{ 36,82,145,158},{ 36,83,151,158},{ 36,86,157,157},{ 36,87,162,157},{ 36,89,168,156},{ 36,92,174,155},{ 36,93,179,154},{ 36,96,185,154},{ 36,98,190,153},
		{ 36,76,108,161},{ 36,77,110,160},{ 36,77,113,160},{ 36,79,117,160},{ 36,80,122,159},{ 36,81,127,158},{ 36,83,132,158},{ 36,84,137,157},{ 36,85,143,156},{ 36,87,149,156},{ 36,89,154,155},{ 36,90,161,154},{ 36,93,167,154},{ 36,95,172,153},{ 36,97,178,152},{ 36,99,183,152},{ 36,101,188,151},
		{ 36,80,107,159},{ 36,81,108,158},{ 36,82,111,158},{ 36,83,115,157},{ 36,85,120,156},{ 36,85,125,156},{ 36,87,130,156},{ 36,88,136,155},{ 36,90,141,154},{ 36,91,147,153},{ 36,93,153,153},{ 24,95,158,152, 12,96,158,152},{ 36,97,164,151},{ 24,99,169,151, 12,100,169,151},{ 36,101,175,150},{ 36,103,180,149},{ 36,105,186,149},
		{ 36,85,105,156},{ 36,86,106,156},{ 36,87,109,155},{ 36,88,113,154},{ 24,89,117,154, 12,90,117,154},{ 36,90,123,153},{ 36,92,128,153},{ 36,93,133,152},{ 36,95,138,152},{ 36,96,144,150},{ 36,98,150,150},{ 36,101,155,149},{ 36,102,161,149},{ 36,104,166,148},{ 36,106,172,148},{ 36,108,177,147},{ 36,110,183,146},
		{ 36,91,103,153},{ 36,92,104,153},{ 36,93,107,152},{ 36,94,110,152},{ 36,95,115,151},{ 36,96,119,150},{ 36,98,125,150},{ 36,99,130,149},{ 36,101,135,149},{ 36,103,141,148},{ 36,104,147,147},{ 36,106,152,146},{ 24,108,158,146, 12,107,158,146},{ 36,110,164,145},{ 36,111,169,145},{ 36,114,175,144},{ 36,116,180,144},
		{ 36,98,101,150},{ 36,98,102,150},{ 36,99,105,149},{ 36,100,108,148},{ 36,101,112,148},{ 36,102,117,147},{ 36,104,122,147},{ 36,105,127,146},{ 36,107,133,145},{ 36,109,138,145},{ 36,110,143,144},{ 36,112,149,143},{ 36,114,155,143},{ 36,116,160,143},{ 28,117,166,142, 8,118,166,142},{ 36,120,171,142},{ 36,121,177,141},
		{ 36,105,99,147},{ 36,105,100,147},{ 36,106,102,146},{ 36,107,105,146},{ 36,108,109,145},{ 36,109,114,144},{ 36,111,119,144},{ 24,112,124,143, 12,113,124,143},{ 36,113,129,143},{ 36,116,134,142},{ 36,117,140,142},{ 36,119,145,140},{ 36,120,151,140},{ 36,122,157,140},{ 36,125,162,139},{ 36,126,167,139},{ 36,129,173,138},
		{ 36,112,97,144},{ 36,112,97,143},{ 36,113,99,143},{ 36,114,102,143},{ 36,115,106,142},{ 36,116,111,141},{ 36,118,115,141},{ 36,120,121,140},{ 36,121,126,140},{ 36,123,131,139},{ 36,124,137,139},{ 36,126,142,138},{ 36,127,148,137},{ 36,129,153,137},{ 36,132,159,136},{ 20,133,164,136, 16,134,164,136},{ 36,136,169,136},
		{ 36,119,94,140},{ 36,119,95,140},{ 36,121,97,140},{ 36,121,100,140},{ 36,123,104,139},{ 36,124,108,139},{ 36,125,113,138},{ 36,127,118,137},{ 36,128,123,137},{ 36,130,128,136},{ 36,131,133,136},{ 36,133,139,135},{ 36,135,144,135},{ 36,137,149,134},{ 36,139,155,134},{ 36,140,160,133},{ 36,142,166,133},
		{ 36,127,92,138},{ 36,127,93,137},{ 36,128,95,137},{ 36,128,97,137},{ 36,130,101,137},{ 36,132,105,136},{ 36,132,109,135},{ 36,134,114,134},{ 36,135,119,134},{ 36,137,124,134},{ 36,138,129,133},{ 20,140,135,133, 16,141,135,133},{ 36,142,140,132},{ 36,144,145,132},{ 36,146,151,131},{ 36,147,156,130},{ 36,150,162,130},
		{ 36,135,90,135},{ 36,135,90,134},{ 36,136,92,134},{ 36,137,94,134},{ 36,138,98,134},{ 36,139,102,133},{ 36,140,106,133},{ 36,142,111,132},{ 36,143,116,131},{ 36,145,121,131},{ 36,147,126,130},{ 36,148,131,130},{ 36,150,137,129},{ 36,151,142,129},{ 36,153,147,128},{ 36,155,153,128},{ 36,157,158,128},
		{ 36,142,88,132},{ 36,142,88,132},{ 36,144,90,132},{ 36,145,92,131},{ 36,145,95,131},{ 36,147,99,131},{ 36,148,103,130},{ 36,150,107,130},{ 36,151,112,129},{ 36,153,117,128},{ 36,155,122,128},{ 36,156,128,128},{ 36,158,133,127},{ 36,159,138,127},{ 36,161,144,126},{ 36,162,149,126},{ 36,165,155,125},
		{ 36,151,85,129},{ 36,151,85,129},{ 36,152,87,129},{ 36,153,89,129},{ 36,154,92,128},{ 36,155,95,128},{ 36,156,99,128},{ 36,157,104,127},{ 36,159,109,127},{ 36,160,113,126},{ 36,162,118,125},{ 36,164,124,125},{ 36,165,129,125},{ 36,167,134,124},{ 36,169,140,124},{ 36,170,145,123},{ 36,172,150,123},
		{ 36,159,83,127},{ 36,160,83,127},{ 36,160,84,126},{ 36,161,86,126},{ 36,162,89,126},{ 36,163,93,125},{ 36,164,96,125},{ 36,166,101,125},{ 36,168,105,124},{ 36,168,110,124},{ 36,170,115,123},{ 36,171,120,123},{ 36,173,126,122},{ 36,174,131,122},{ 36,177,136,122},{ 36,179,141,121},{ 36,180,146,121},
		{ 36,167,80,125},{ 36,168,80,125},{ 36,168,82,124},{ 36,169,84,124},{ 36,170,86,123},{ 36,171,89,123},{ 36,173,93,123},{ 36,174,98,122},{ 28,175,102,122, 8,176,102,122},{ 36,177,106,121},{ 36,178,111,121},{ 36,180,116,120},{ 36,181,122,120},{ 36,183,127,120},{ 36,185,132,119},{ 36,187,137,119},{ 36,188,143,119},
		{ 36,175,78,123},{ 36,176,78,122},{ 36,176,79,122},{ 24,178,81,122, 12,177,81,122},{ 36,178,83,121},{ 36,180,86,121},{ 36,181,90,120},{ 36,182,94,120},{ 24,183,98,120, 12,184,98,120},{ 36,185,103,119},{ 36,186,108,119},{ 28,188,113,118, 8,187,113,118},{ 32,190,118,118, 4,189,118,118},{ 36,191,123,117},{ 36,193,128,117},{ 36,195,134,117},{ 36,196,139,117},
		{ 36,184,75,120},{ 24,183,76,120, 12,184,76,120},{ 36,185,77,120},{ 36,185,78,119},{ 36,186,81,119},{ 36,188,83,119},{ 36,189,87,119},{ 36,190,91,118},{ 36,191,95,117},{ 36,193,100,117},{ 36,194,104,117},{ 36,196,109,116},{ 36,198,114,116},{ 36,199,120,116},{ 36,201,125,115},{ 36,202,129,115},{ 36,204,135,114},
		{ 36,79,108,166},{ 36,80,109,166},{ 36,80,112,165},{ 36,81,117,165},{ 36,83,121,164},{ 36,83,127,164},{ 36,85,132,163},{ 36,86,137,162},{ 36,88,143,162},{ 36,89,149,161},{ 36,91,154,160},{ 36,93,160,160},{ 36,94,166,159},{ 36,97,171,159},{ 36,98,177,157},{ 36,100,182,157},{ 36,102,188,156},
		{ 36,82,107,164},{ 24,82,108,164, 12,83,108,164},{ 36,84,111,163},{ 36,84,115,163},{ 36,85,120,162},{ 24,87,125,162, 12,86,125,162},{ 36,88,130,161},{ 36,89,135,160},{ 36,91,141,160},{ 36,92,147,159},{ 36,94,152,158},{ 36,96,158,158},{ 20,98,163,157, 16,97,163,157},{ 36,100,169,156},{ 36,102,174,156},{ 36,104,180,155},{ 36,105,186,154},
		{ 36,86,105,162},{ 36,86,106,161},{ 36,88,109,161},{ 36,88,113,160},{ 36,90,118,160},{ 36,90,122,159},{ 36,92,128,159},{ 36,93,132,158},{ 36,95,138,157},{ 36,97,144,156},{ 36,98,150,156},{ 36,100,155,155},{ 36,102,162,155},{ 36,104,167,154},{ 36,106,173,153},{ 36,108,178,152},{ 24,109,183,152, 12,110,183,152},
		{ 36,91,103,159},{ 36,91,105,159},{ 36,92,107,158},{ 36,93,111,158},{ 36,94,116,157},{ 36,95,121,156},{ 36,97,126,156},{ 36,98,131,155},{ 36,100,136,155},{ 36,102,142,154},{ 36,103,147,153},{ 36,105,153,153},{ 36,107,159,152},{ 36,109,164,151},{ 36,110,170,151},{ 36,113,175,150},{ 36,114,181,150},
		{ 36,97,102,156},{ 36,97,103,156},{ 36,98,105,156},{ 36,98,109,155},{ 36,100,113,154},{ 36,101,118,154},{ 36,102,123,153},{ 36,104,128,153},{ 36,105,133,152},{ 36,107,139,151},{ 36,108,144,150},{ 36,110,150,150},{ 36,112,155,149},{ 36,114,161,149},{ 20,117,167,148, 16,116,167,148},{ 36,118,172,148},{ 36,121,178,147},
		{ 36,103,99,153},{ 36,103,101,153},{ 36,104,103,153},{ 36,104,106,152},{ 36,106,110,151},{ 36,107,115,151},{ 36,109,120,150},{ 36,110,125,149},{ 36,111,130,149},{ 36,113,135,148},{ 36,114,142,148},{ 24,116,147,147, 12,117,147,147},{ 36,118,153,146},{ 36,120,158,146},{ 28,122,164,145, 8,123,164,145},{ 36,124,170,145},{ 36,126,174,144},
		{ 36,109,97,150},{ 36,109,99,150},{ 36,110,101,149},{ 36,111,104,149},{ 36,112,108,148},{ 36,114,112,148},{ 36,115,117,147},{ 36,117,122,146},{ 36,118,128,146},{ 36,120,133,145},{ 36,121,138,145},{ 36,123,144,144},{ 36,125,149,143},{ 36,127,155,143},{ 36,129,160,142},{ 36,130,166,142},{ 36,133,171,142},
		{ 36,116,95,147},{ 36,116,96,147},{ 36,117,98,147},{ 36,118,101,146},{ 36,119,105,145},{ 36,121,109,145},{ 36,122,114,144},{ 32,124,119,143, 4,123,119,143},{ 36,124,124,143},{ 36,127,129,143},{ 36,128,135,142},{ 36,130,140,141},{ 36,132,145,140},{ 36,133,151,140},{ 36,135,156,139},{ 36,137,162,139},{ 36,139,168,139},
		{ 36,123,93,144},{ 20,123,94,144, 16,124,94,144},{ 28,124,96,143, 8,125,96,143},{ 36,126,98,143},{ 36,127,102,143},{ 36,128,106,142},{ 36,129,110,141},{ 36,130,115,141},{ 36,132,121,140},{ 36,134,126,140},{ 36,136,131,139},{ 36,137,137,139},{ 36,139,142,138},{ 36,140,148,137},{ 36,142,153,137},{ 36,144,159,136},{ 36,146,164,136},
		{ 36,130,91,141},{ 24,130,92,141, 12,131,92,141},{ 36,132,93,140},{ 36,133,96,140},{ 36,134,100,140},{ 36,135,104,139},{ 36,136,108,139},{ 36,138,113,138},{ 36,139,118,137},{ 36,141,123,137},{ 36,143,128,136},{ 36,144,133,136},{ 36,146,138,135},{ 36,147,144,135},{ 36,150,149,134},{ 36,151,155,134},{ 36,153,160,133},
		{ 36,138,89,138},{ 36,139,89,138},{ 36,139,91,137},{ 36,141,93,137},{ 36,141,96,137},{ 36,143,100,136},{ 36,144,104,136},{ 36,145,109,135},{ 36,146,114,134},{ 36,148,119,134},{ 36,150,124,134},{ 36,151,129,133},{ 36,153,135,133},{ 36,154,140,132},{ 36,157,145,132},{ 36,158,151,131},{ 36,160,156,130},
		{ 36,146,87,136},{ 36,147,87,135},{ 36,147,88,135},{ 36,148,90,134},{ 36,149,93,134},{ 36,150,97,134},{ 36,151,101,133},{ 36,153,106,133},{ 36,155,111,132},{ 36,156,116,131},{ 36,158,121,131},{ 36,159,126,130},{ 36,161,131,130},{ 36,162,137,129},{ 36,164,142,129},{ 36,166,147,128},{ 36,168,152,128},
		{ 36,154,84,133},{ 24,154,85,133, 12,155,85,133},{ 36,155,86,132},{ 36,156,88,132},{ 36,157,91,132},{ 36,158,94,131},{ 36,159,98,130},{ 36,161,103,130},{ 36,162,107,130},{ 36,164,112,129},{ 36,165,117,128},{ 36,167,122,128},{ 36,169,128,128},{ 36,170,133,127},{ 36,172,138,127},{ 36,174,144,126},{ 36,175,149,126},
		{ 36,162,82,130},{ 36,163,82,130},{ 36,163,83,129},{ 36,164,85,129},{ 36,165,88,129},{ 36,166,91,128},{ 36,168,95,128},{ 36,168,99,128},{ 36,170,104,127},{ 36,171,108,127},{ 36,173,113,126},{ 36,174,118,125},{ 36,176,124,125},{ 36,178,129,125},{ 36,179,134,124},{ 36,182,140,124},{ 36,183,145,123},
		{ 36,170,79,127},{ 36,171,80,127},{ 36,171,81,127},{ 24,172,82,126, 12,173,82,126},{ 36,173,85,126},{ 36,174,88,126},{ 36,176,92,125},{ 36,177,96,125},{ 36,179,101,125},{ 36,179,105,124},{ 36,181,110,124},{ 36,182,115,123},{ 36,184,120,123},{ 36,186,126,122},{ 36,188,131,122},{ 36,190,136,121},{ 36,191,141,121},
		{ 36,178,77,125},{ 36,179,77,125},{ 36,179,78,125},{ 32,181,80,125, 4,180,80,125},{ 36,182,82,124},{ 36,183,85,123},{ 36,184,89,123},{ 36,185,93,123},{ 36,186,97,122},{ 36,188,102,122},{ 36,189,106,121},{ 36,191,111,121},{ 36,192,116,120},{ 36,194,122,120},{ 24,196,127,120, 12,195,127,120},{ 36,198,132,119},{ 36,199,137,119},
		{ 36,187,74,123},{ 36,186,75,123},{ 36,188,75,123},{ 36,189,77,122},{ 36,189,79,122},{ 36,191,82,122},{ 36,191,86,121},{ 36,193,90,121},{ 36,194,94,120},{ 36,196,98,120},{ 36,197,103,119},{ 36,198,108,119},{ 36,200,113,119},{ 36,202,118,118},{ 36,204,124,118},{ 36,205,128,117},{ 36,207,133,117},
		{ 36,85,106,169},{ 24,85,107,169, 12,86,107,169},{ 36,86,110,168},{ 36,87,114,168},{ 36,88,118,167},{ 36,89,124,167},{ 36,90,129,166},{ 36,91,134,165},{ 36,93,140,165},{ 36,95,145,164},{ 36,96,151,163},{ 36,98,157,163},{ 36,99,163,162},{ 36,102,168,162},{ 36,103,174,160},{ 36,105,180,160},{ 36,107,185,159},
		{ 36,88,105,167},{ 36,88,106,167},{ 36,89,109,166},{ 36,90,113,166},{ 36,91,118,165},{ 36,92,123,165},{ 20,94,127,164, 16,93,127,164},{ 36,94,133,164},{ 36,96,139,163},{ 36,98,144,162},{ 36,99,150,161},{ 36,101,155,161},{ 24,102,161,160, 12,103,161,160},{ 36,105,167,160},{ 36,106,172,159},{ 36,108,178,158},{ 36,110,183,157},
		{ 36,92,103,165},{ 36,92,105,164},{ 36,93,107,164},{ 36,94,111,164},{ 36,95,115,163},{ 36,96,120,163},{ 36,97,125,162},{ 36,99,130,161},{ 36,100,136,160},{ 36,102,142,160},{ 36,103,147,159},{ 36,105,153,159},{ 36,106,159,158},{ 36,109,164,157},{ 36,110,170,157},{ 36,112,175,156},{ 36,115,181,155},
		{ 36,97,102,163},{ 36,97,103,162},{ 36,98,105,161},{ 36,98,109,161},{ 36,99,113,161},{ 36,100,118,160},{ 36,102,123,159},{ 36,104,128,159},{ 36,105,133,158},{ 36,106,139,157},{ 20,108,144,157, 16,107,144,157},{ 36,110,150,156},{ 36,111,156,155},{ 36,113,162,155},{ 36,116,168,154},{ 36,117,173,153},{ 36,120,178,153},
		{ 36,102,100,160},{ 36,102,101,159},{ 36,103,104,159},{ 36,103,107,158},{ 36,105,111,158},{ 36,106,116,157},{ 36,107,121,156},{ 36,109,126,156},{ 36,110,131,155},{ 36,112,137,155},{ 36,113,142,154},{ 36,115,148,153},{ 36,116,153,153},{ 24,118,159,152, 12,119,159,152},{ 36,121,165,151},{ 36,122,170,151},{ 36,125,176,150},
		{ 24,107,98,157, 12,108,98,157},{ 36,108,99,156},{ 24,108,101,156, 12,109,101,156},{ 36,109,105,156},{ 36,110,109,154},{ 36,112,113,154},{ 36,113,118,154},{ 36,115,123,153},{ 36,116,128,152},{ 24,117,133,152, 12,118,133,152},{ 36,119,139,151},{ 36,121,144,150},{ 36,123,150,150},{ 36,124,156,149},{ 36,127,161,149},{ 36,128,167,148},{ 36,130,172,148},
		{ 36,114,96,153},{ 36,114,97,153},{ 36,115,99,153},{ 36,116,102,153},{ 36,117,106,152},{ 36,118,110,151},{ 36,119,115,151},{ 36,121,120,150},{ 36,122,125,149},{ 36,124,130,149},{ 36,125,135,148},{ 36,127,142,148},{ 36,129,147,147},{ 36,131,153,146},{ 36,133,158,145},{ 36,134,164,145},{ 36,136,169,145},
		{ 36,120,94,151},{ 36,120,95,150},{ 36,122,97,150},{ 36,123,100,149},{ 36,124,103,149},{ 36,125,108,148},{ 36,126,112,147},{ 36,128,117,147},{ 36,129,122,146},{ 36,130,128,146},{ 36,133,133,145},{ 36,134,138,145},{ 36,136,144,144},{ 36,137,149,143},{ 36,139,155,143},{ 32,141,160,142, 4,140,160,142},{ 36,143,166,142},
		{ 36,127,92,147},{ 36,128,92,147},{ 36,129,94,147},{ 36,130,97,146},{ 36,131,100,146},{ 36,132,105,145},{ 36,133,109,144},{ 36,135,114,144},{ 36,136,119,143},{ 36,137,124,143},{ 36,139,129,143},{ 36,140,134,142},{ 24,142,140,141, 12,143,140,141},{ 36,144,145,140},{ 36,146,151,140},{ 36,147,157,139},{ 36,150,162,139},
		{ 36,135,90,144},{ 36,136,90,144},{ 36,136,92,144},{ 36,137,94,143},{ 36,138,98,143},{ 36,139,101,143},{ 36,140,106,142},{ 36,142,110,141},{ 36,144,115,140},{ 36,144,121,140},{ 36,146,126,140},{ 36,147,131,139},{ 36,150,137,138},{ 36,151,142,138},{ 36,153,148,137},{ 36,155,153,137},{ 36,156,158,136},
		{ 36,142,88,141},{ 36,143,88,141},{ 36,143,90,141},{ 36,144,92,140},{ 36,145,95,140},{ 36,147,99,140},{ 36,147,103,139},{ 36,149,108,138},{ 36,151,113,138},{ 36,152,117,137},{ 36,154,123,136},{ 36,155,128,136},{ 36,157,133,136},{ 36,158,138,135},{ 36,160,144,135},{ 36,162,149,134},{ 36,164,155,134},
		{ 36,150,85,139},{ 36,150,86,138},{ 36,151,87,138},{ 36,152,89,138},{ 36,153,92,137},{ 36,154,95,137},{ 36,156,100,136},{ 36,156,104,136},{ 36,158,109,135},{ 36,159,114,134},{ 36,161,119,134},{ 36,162,124,134},{ 36,164,129,133},{ 36,166,135,133},{ 36,167,140,132},{ 20,170,145,132, 16,169,145,132},{ 36,171,151,131},
		{ 36,157,83,136},{ 36,158,83,136},{ 36,158,85,135},{ 36,160,87,135},{ 36,160,89,134},{ 36,162,93,134},{ 36,163,96,134},{ 36,164,101,133},{ 36,166,106,133},{ 36,167,111,132},{ 36,169,116,131},{ 36,170,121,131},{ 36,172,126,130},{ 36,174,131,130},{ 36,175,137,129},{ 36,177,142,129},{ 36,178,147,129},
		{ 36,165,81,133},{ 36,166,81,133},{ 36,166,82,133},{ 36,168,84,132},{ 36,169,87,132},{ 36,170,90,131},{ 36,171,94,131},{ 36,172,98,131},{ 32,174,103,130, 4,173,103,130},{ 36,174,107,130},{ 36,176,112,129},{ 28,178,117,128, 8,179,117,128},{ 36,179,122,128},{ 36,182,128,128},{ 24,182,133,127, 12,183,133,127},{ 36,185,138,127},{ 36,186,143,126},
		{ 36,173,78,131},{ 36,174,79,130},{ 36,174,80,130},{ 36,176,81,129},{ 36,177,84,129},{ 20,178,87,129, 16,177,87,129},{ 36,179,91,128},{ 36,180,94,128},{ 36,182,99,127},{ 36,182,104,127},{ 36,184,108,127},{ 36,186,113,126},{ 36,187,118,125},{ 36,189,124,125},{ 36,190,129,125},{ 36,192,134,124},{ 36,194,140,124},
		{ 36,181,76,128},{ 36,182,76,128},{ 36,183,77,127},{ 36,184,78,127},{ 36,185,81,127},{ 36,185,84,126},{ 36,187,88,126},{ 36,188,92,126},{ 36,190,96,125},{ 36,191,101,125},{ 36,192,105,124},{ 36,194,110,124},{ 36,195,115,123},{ 36,197,120,123},{ 28,198,125,122, 8,199,125,122},{ 36,201,131,122},{ 36,202,135,121},
		{ 36,189,74,126},{ 36,191,74,125},{ 36,191,75,125},{ 36,192,76,125},{ 36,192,78,125},{ 36,194,81,124},{ 36,194,84,124},{ 36,196,88,123},{ 36,197,92,123},{ 36,199,97,122},{ 36,201,101,122},{ 36,202,106,121},{ 28,203,111,121, 8,204,111,121},{ 36,205,116,121},{ 36,206,122,120},{ 36,208,127,120},{ 36,210,132,119},
		{ 36,91,104,172},{ 36,91,105,171},{ 36,92,108,171},{ 24,92,112,171, 12,93,112,171},{ 36,94,116,170},{ 36,94,121,170},{ 36,96,127,169},{ 36,96,132,168},{ 36,98,137,168},{ 36,100,143,167},{ 36,101,149,167},{ 36,103,154,166},{ 36,104,160,165},{ 36,107,166,164},{ 36,108,171,164},{ 36,110,177,163},{ 36,112,182,162},
		{ 36,94,103,170},{ 36,94,104,169},{ 36,95,107,169},{ 36,95,111,169},{ 36,97,115,169},{ 36,97,120,168},{ 36,99,125,167},{ 36,100,130,167},{ 36,101,135,166},{ 36,103,141,165},{ 36,104,147,165},{ 36,106,152,164},{ 36,108,158,163},{ 36,110,164,163},{ 36,112,169,162},{ 36,113,176,161},{ 36,116,180,160},
		{ 36,98,102,168},{ 36,97,103,167},{ 36,98,106,167},{ 36,99,109,167},{ 36,100,113,166},{ 36,101,118,166},{ 36,102,123,165},{ 36,104,128,165},{ 36,105,134,164},{ 36,107,140,163},{ 36,108,145,162},{ 36,110,151,162},{ 36,111,156,161},{ 36,114,162,160},{ 36,116,168,160},{ 36,117,173,159},{ 36,119,178,158},
		{ 36,102,100,166},{ 36,102,101,165},{ 36,103,104,165},{ 36,103,107,164},{ 36,105,111,164},{ 36,106,116,163},{ 36,107,121,163},{ 36,108,126,162},{ 36,110,131,161},{ 36,111,137,160},{ 36,112,142,160},{ 36,114,148,159},{ 36,116,153,159},{ 36,118,159,158},{ 36,120,165,157},{ 36,122,170,157},{ 36,124,176,156},
		{ 36,107,99,163},{ 36,107,99,163},{ 36,108,102,162},{ 36,108,105,161},{ 36,110,109,161},{ 36,111,113,160},{ 36,112,118,160},{ 36,113,123,159},{ 36,115,128,159},{ 36,116,134,158},{ 36,117,139,157},{ 36,119,145,156},{ 36,122,150,156},{ 36,123,156,155},{ 36,125,162,155},{ 36,127,168,154},{ 36,129,173,154},
		{ 36,112,97,160},{ 36,112,98,160},{ 36,113,100,159},{ 36,115,103,159},{ 36,115,107,158},{ 36,117,111,157},{ 36,118,116,157},{ 36,119,121,156},{ 36,120,126,156},{ 36,122,131,155},{ 36,124,137,155},{ 36,125,142,153},{ 36,127,148,153},{ 36,129,154,152},{ 36,131,159,152},{ 28,132,165,151, 8,133,165,151},{ 36,135,169,151},
		{ 36,118,95,157},{ 36,118,96,157},{ 36,119,97,156},{ 36,121,100,156},{ 36,121,104,155},{ 36,123,109,154},{ 36,124,113,154},{ 36,125,118,153},{ 36,126,123,153},{ 36,128,128,152},{ 36,130,133,152},{ 36,132,139,151},{ 36,133,145,150},{ 36,135,150,149},{ 36,137,156,149},{ 36,138,161,148},{ 36,141,167,148},
		{ 36,125,93,154},{ 36,126,93,153},{ 36,126,95,153},{ 36,127,98,153},{ 36,128,101,152},{ 36,129,106,151},{ 36,130,110,151},{ 36,132,115,150},{ 36,134,120,150},{ 36,135,125,149},{ 24,137,130,149, 12,136,130,149},{ 36,138,135,148},{ 36,140,142,148},{ 36,141,147,147},{ 36,143,153,146},{ 36,145,158,145},{ 36,147,163,145},
		{ 36,132,91,151},{ 20,133,91,151, 16,132,91,151},{ 36,133,93,150},{ 36,134,96,150},{ 36,135,99,149},{ 36,136,103,149},{ 36,137,107,148},{ 36,139,112,147},{ 36,141,117,147},{ 36,141,122,146},{ 36,143,128,146},{ 36,144,133,145},{ 36,146,138,145},{ 36,148,144,144},{ 36,150,149,143},{ 36,152,155,143},{ 36,154,160,142},
		{ 36,139,89,148},{ 36,140,89,147},{ 36,140,91,147},{ 36,141,93,147},{ 36,142,96,146},{ 36,143,100,146},{ 36,145,104,145},{ 36,146,109,144},{ 36,147,114,144},{ 36,148,119,143},{ 36,150,124,143},{ 36,151,129,142},{ 36,153,135,142},{ 36,154,140,141},{ 36,157,145,140},{ 36,159,151,140},{ 36,160,156,139},
		{ 36,146,86,145},{ 36,147,87,145},{ 36,147,88,144},{ 36,148,90,144},{ 36,149,93,143},{ 36,150,97,143},{ 36,152,101,142},{ 36,153,106,142},{ 36,155,110,141},{ 32,156,115,140, 4,155,115,140},{ 36,157,121,140},{ 36,158,126,140},{ 36,160,131,139},{ 36,162,137,139},{ 36,164,142,138},{ 36,166,148,137},{ 36,167,152,136},
		{ 36,154,84,142},{ 36,154,85,141},{ 36,154,86,141},{ 36,156,88,141},{ 36,157,91,140},{ 36,158,95,140},{ 36,159,99,139},{ 36,160,103,139},{ 36,162,108,138},{ 36,163,113,138},{ 36,165,118,137},{ 36,166,123,137},{ 36,168,128,136},{ 36,170,133,136},{ 36,171,139,135},{ 36,173,144,135},{ 36,174,149,134},
		{ 36,161,82,139},{ 36,162,82,139},{ 36,162,84,138},{ 36,163,85,138},{ 36,165,88,138},{ 36,165,91,137},{ 36,167,95,137},{ 36,167,100,136},{ 36,169,104,136},{ 36,170,109,135},{ 36,172,114,134},{ 36,174,119,134},{ 36,175,124,133},{ 36,177,129,133},{ 36,178,135,133},{ 36,180,140,132},{ 36,182,145,132},
		{ 36,169,80,136},{ 36,170,80,136},{ 36,171,81,136},{ 36,171,83,135},{ 24,173,85,135, 12,172,85,135},{ 36,173,88,134},{ 36,174,92,134},{ 36,175,96,134},{ 36,177,101,133},{ 36,178,106,133},{ 36,180,111,132},{ 36,182,116,131},{ 36,183,121,131},{ 36,185,126,130},{ 36,186,131,130},{ 36,188,137,129},{ 36,189,142,129},
		{ 36,177,77,134},{ 36,177,78,134},{ 28,178,78,133, 8,179,78,133},{ 36,179,80,133},{ 36,180,83,132},{ 36,181,86,132},{ 36,182,89,131},{ 36,183,93,131},{ 36,185,98,131},{ 36,186,102,130},{ 36,187,107,130},{ 36,189,112,129},{ 20,190,117,128, 16,191,117,128},{ 36,192,122,128},{ 36,194,128,128},{ 24,196,133,127, 12,195,133,127},{ 36,197,138,127},
		{ 36,185,75,131},{ 32,186,75,131, 4,185,75,131},{ 36,187,76,131},{ 36,187,77,130},{ 36,188,80,129},{ 36,189,83,129},{ 36,190,86,129},{ 36,191,90,128},{ 36,193,94,128},{ 36,194,99,128},{ 36,195,104,127},{ 36,197,108,127},{ 36,198,113,126},{ 36,201,118,125},{ 36,202,124,125},{ 36,204,129,125},{ 36,205,134,124},
		{ 24,193,73,129, 12,192,73,129},{ 36,194,73,128},{ 36,194,74,128},{ 36,195,75,127},{ 36,195,77,127},{ 36,197,80,127},{ 36,197,83,127},{ 36,199,87,126},{ 36,201,91,126},{ 36,202,96,125},{ 36,204,100,125},{ 36,205,105,124},{ 36,206,110,124},{ 36,208,115,123},{ 36,209,120,123},{ 36,212,125,122},{ 36,213,131,122},
		{ 36,97,102,175},{ 36,97,104,174},{ 36,98,106,174},{ 36,98,109,174},{ 36,99,114,173},{ 36,100,119,173},{ 24,102,124,172, 12,101,124,172},{ 36,103,129,171},{ 24,104,135,171, 12,103,135,171},{ 24,105,141,170, 12,106,141,170},{ 36,107,146,170},{ 36,109,152,169},{ 36,110,157,168},{ 36,112,163,168},{ 20,114,169,166, 16,113,169,166},{ 36,115,175,166},{ 36,117,179,165},
		{ 36,100,102,173},{ 28,99,103,173, 8,100,103,173},{ 36,100,105,172},{ 36,101,109,172},{ 36,102,113,171},{ 36,103,118,171},{ 36,104,123,170},{ 36,106,128,170},{ 36,106,133,169},{ 36,108,139,169},{ 36,109,144,168},{ 36,111,150,167},{ 36,113,156,167},{ 36,115,161,166},{ 36,117,167,165},{ 36,118,173,164},{ 24,120,178,164, 12,121,178,164},
		{ 36,103,100,171},{ 36,103,101,171},{ 36,104,104,170},{ 36,104,107,170},{ 24,105,111,169, 12,106,111,169},{ 36,107,116,169},{ 36,108,120,168},{ 36,109,126,168},{ 36,110,131,167},{ 24,111,137,166, 12,112,137,166},{ 36,113,142,166},{ 36,115,147,165},{ 36,117,153,164},{ 36,118,159,164},{ 36,121,164,163},{ 36,122,170,162},{ 36,124,176,161},
		{ 36,107,99,169},{ 36,107,100,169},{ 36,108,102,168},{ 36,109,105,167},{ 36,110,109,167},{ 36,111,114,166},{ 36,112,119,166},{ 36,114,124,165},{ 24,114,129,165, 12,115,129,165},{ 36,116,135,164},{ 36,117,140,163},{ 36,119,146,162},{ 36,121,151,162},{ 36,123,157,161},{ 36,125,163,160},{ 36,126,168,160},{ 36,129,173,159},
		{ 36,112,97,166},{ 36,112,98,166},{ 36,113,100,165},{ 36,114,103,165},{ 24,114,107,164, 12,115,107,164},{ 36,116,111,164},{ 36,117,116,163},{ 36,119,121,162},{ 36,119,127,162},{ 36,121,132,161},{ 36,123,137,160},{ 36,124,143,160},{ 36,126,148,159},{ 36,128,154,159},{ 36,130,160,158},{ 36,131,165,157},{ 36,134,171,157},
		{ 36,117,95,163},{ 36,118,96,163},{ 36,118,98,163},{ 36,120,101,162},{ 36,120,105,161},{ 36,122,109,161},{ 36,122,113,160},{ 36,124,118,160},{ 36,125,124,159},{ 36,127,129,159},{ 36,129,134,158},{ 36,130,140,157},{ 36,132,145,156},{ 36,133,150,156},{ 36,135,156,155},{ 36,137,163,155},{ 36,139,167,154},
		{ 36,123,93,160},{ 36,124,94,160},{ 36,124,96,160},{ 24,126,99,159, 12,125,99,159},{ 36,126,102,159},{ 36,127,107,158},{ 36,128,111,157},{ 36,130,116,157},{ 36,132,121,156},{ 36,133,127,156},{ 36,135,132,155},{ 36,136,137,154},{ 36,138,143,154},{ 36,139,148,153},{ 36,141,154,152},{ 36,143,159,152},{ 36,145,164,151},
		{ 36,130,91,157},{ 24,130,92,157, 12,131,92,157},{ 36,130,94,157},{ 36,132,96,156},{ 36,132,100,156},{ 36,134,104,155},{ 36,135,108,154},{ 36,136,113,154},{ 36,138,118,153},{ 36,139,123,153},{ 36,141,128,152},{ 36,142,134,151},{ 36,144,139,151},{ 36,145,145,150},{ 36,147,150,149},{ 36,150,156,149},{ 36,151,161,149},
		{ 36,136,89,154},{ 36,137,90,154},{ 36,137,92,153},{ 36,139,94,153},{ 36,139,97,153},{ 36,141,101,152},{ 36,142,105,151},{ 36,143,110,151},{ 36,144,115,150},{ 36,145,120,150},{ 36,147,125,149},{ 36,148,130,149},{ 36,150,136,148},{ 36,153,142,147},{ 36,154,148,147},{ 36,156,153,146},{ 36,157,158,145},
		{ 36,143,87,151},{ 36,144,88,151},{ 36,144,90,151},{ 36,145,92,150},{ 36,146,95,150},{ 36,147,99,149},{ 36,149,103,149},{ 36,150,107,148},{ 36,151,112,147},{ 36,152,117,147},{ 36,154,123,146},{ 36,155,128,146},{ 36,157,133,145},{ 36,159,138,145},{ 36,160,144,144},{ 36,163,149,143},{ 36,164,155,143},
		{ 36,150,85,149},{ 36,151,86,148},{ 36,151,87,147},{ 36,152,89,147},{ 36,154,92,147},{ 36,154,96,146},{ 36,156,100,146},{ 36,157,104,145},{ 36,158,109,144},{ 36,159,114,144},{ 36,161,119,143},{ 36,163,124,143},{ 36,164,129,142},{ 36,166,135,142},{ 36,167,140,141},{ 36,169,145,140},{ 24,171,151,140, 12,170,151,140},
		{ 36,157,83,146},{ 36,158,83,145},{ 36,158,85,144},{ 36,160,87,144},{ 36,161,89,144},{ 36,161,93,143},{ 36,163,97,143},{ 36,164,101,142},{ 36,165,105,142},{ 24,166,110,141, 12,167,110,141},{ 36,168,115,140},{ 36,170,121,140},{ 36,171,126,139},{ 36,173,132,139},{ 36,174,137,139},{ 36,176,142,138},{ 36,178,147,137},
		{ 36,165,81,142},{ 36,166,81,142},{ 36,167,82,142},{ 36,167,84,141},{ 36,168,87,141},{ 36,169,90,140},{ 36,170,94,140},{ 36,171,98,139},{ 36,173,103,139},{ 36,175,107,138},{ 36,176,113,138},{ 28,177,118,137, 8,178,118,137},{ 36,179,123,137},{ 36,180,128,136},{ 36,182,133,136},{ 36,184,139,135},{ 36,185,144,135},
		{ 36,172,78,140},{ 36,173,79,139},{ 36,174,80,139},{ 36,174,82,138},{ 36,176,84,138},{ 36,176,87,138},{ 36,178,91,137},{ 36,179,95,137},{ 36,180,99,136},{ 36,182,104,136},{ 36,183,109,135},{ 36,185,114,134},{ 36,186,119,134},{ 36,188,124,134},{ 36,189,129,133},{ 36,191,135,133},{ 36,192,140,132},
		{ 36,181,76,137},{ 36,181,76,136},{ 36,182,77,136},{ 36,182,79,136},{ 36,183,81,135},{ 36,184,84,135},{ 36,186,88,134},{ 24,187,92,134, 12,188,92,134},{ 36,188,96,134},{ 36,190,100,133},{ 36,191,106,133},{ 36,193,111,132},{ 36,194,116,131},{ 36,196,121,131},{ 36,197,126,130},{ 36,199,131,130},{ 36,201,136,130},
		{ 36,189,74,134},{ 36,189,74,134},{ 36,190,75,134},{ 36,190,77,133},{ 36,191,79,133},{ 36,192,82,132},{ 36,193,85,132},{ 36,195,89,131},{ 36,196,93,131},{ 36,198,98,131},{ 36,199,102,130},{ 24,200,107,130, 12,201,107,130},{ 36,202,112,129},{ 36,204,117,128},{ 36,206,122,128},{ 36,207,128,128},{ 36,209,133,127},
		{ 36,196,71,131},{ 36,197,72,131},{ 36,197,73,131},{ 36,198,74,130},{ 36,199,76,130},{ 36,200,79,130},{ 36,202,82,129},{ 36,202,85,129},{ 36,204,89,128},{ 36,205,94,128},{ 36,207,99,128},{ 36,208,104,127},{ 20,210,109,127, 16,209,109,127},{ 36,212,114,126},{ 36,213,119,126},{ 24,214,124,125, 12,215,124,125},{ 36,216,129,125},
		{ 36,102,101,178},{ 36,103,102,178},{ 36,103,104,177},{ 36,104,108,177},{ 24,104,112,176, 12,105,112,176},{ 36,105,117,176},{ 36,106,122,175},{ 36,107,127,174},{ 36,109,132,174},{ 24,110,137,173, 12,111,137,173},{ 36,112,143,173},{ 36,113,149,172},{ 28,115,154,171, 8,114,154,171},{ 36,117,161,171},{ 36,118,166,170},{ 36,120,172,169},{ 36,122,177,169},
		{ 36,105,100,176},{ 36,105,101,176},{ 36,106,104,176},{ 36,106,107,175},{ 20,108,111,175, 16,107,111,175},{ 36,108,116,174},{ 36,109,121,173},{ 36,111,126,173},{ 36,112,131,172},{ 36,114,136,171},{ 36,115,142,171},{ 36,117,147,170},{ 36,117,153,169},{ 36,119,159,169},{ 36,121,164,168},{ 36,123,170,167},{ 36,125,175,167},
		{ 36,108,99,174},{ 36,108,100,174},{ 36,109,102,173},{ 24,109,105,173, 12,110,105,173},{ 36,111,109,172},{ 36,112,113,172},{ 24,112,119,171, 12,113,119,171},{ 36,114,124,170},{ 36,115,129,170},{ 36,117,134,169},{ 36,118,140,169},{ 36,120,145,168},{ 36,121,151,168},{ 36,123,157,166},{ 36,125,162,166},{ 36,127,168,165},{ 36,129,174,164},
		{ 36,112,97,172},{ 36,112,98,171},{ 36,113,100,171},{ 36,114,103,171},{ 36,115,107,170},{ 36,116,111,169},{ 36,117,116,169},{ 36,118,122,168},{ 36,119,127,168},{ 36,121,132,167},{ 36,122,137,166},{ 36,124,143,166},{ 28,125,149,165, 8,126,149,165},{ 24,127,155,164, 12,128,155,164},{ 36,130,160,163},{ 36,131,166,163},{ 36,133,171,162},
		{ 36,117,96,169},{ 36,117,97,169},{ 36,118,99,168},{ 36,118,102,168},{ 36,120,105,167},{ 36,121,110,167},{ 36,122,114,166},{ 36,123,119,165},{ 36,124,125,165},{ 36,126,130,164},{ 36,127,135,164},{ 36,129,141,163},{ 36,131,146,162},{ 36,132,152,162},{ 36,135,157,161},{ 32,136,163,160, 4,135,163,160},{ 36,138,169,160},
		{ 36,122,94,167},{ 36,122,95,166},{ 36,123,97,165},{ 36,124,100,165},{ 36,125,103,165},{ 24,127,107,164, 12,126,107,164},{ 36,127,112,163},{ 36,129,116,163},{ 36,130,122,162},{ 36,131,127,161},{ 36,132,132,161},{ 36,134,138,160},{ 36,136,143,160},{ 36,138,149,159},{ 36,140,154,158},{ 36,141,160,158},{ 36,143,166,157},
		{ 36,128,92,164},{ 36,128,93,163},{ 36,129,94,162},{ 36,130,97,162},{ 36,131,100,162},{ 36,132,105,161},{ 36,133,109,161},{ 36,135,114,160},{ 32,136,119,160, 4,135,119,160},{ 36,137,124,158},{ 36,139,130,158},{ 36,140,135,157},{ 36,142,141,157},{ 36,143,146,156},{ 24,146,152,156, 12,145,152,156},{ 36,147,157,155},{ 36,149,162,155},
		{ 36,134,90,160},{ 36,135,91,160},{ 36,135,93,160},{ 36,136,95,159},{ 36,137,99,159},{ 24,139,102,158, 12,138,102,158},{ 36,139,107,158},{ 36,141,111,157},{ 36,142,116,157},{ 36,143,122,156},{ 36,145,127,155},{ 36,146,132,155},{ 36,148,137,154},{ 36,149,143,153},{ 36,151,148,153},{ 36,153,154,152},{ 36,155,159,152},
		{ 24,141,88,158, 12,140,88,158},{ 24,142,89,157, 12,141,89,157},{ 36,142,90,157},{ 36,143,93,156},{ 36,143,96,156},{ 36,145,100,155},{ 36,145,104,155},{ 36,147,108,154},{ 36,149,113,154},{ 36,150,118,153},{ 36,152,123,153},{ 36,152,128,151},{ 36,154,134,151},{ 36,156,139,150},{ 36,158,145,150},{ 36,160,150,149},{ 36,161,156,149},
		{ 36,147,86,154},{ 36,148,87,154},{ 36,148,88,154},{ 36,150,90,153},{ 36,150,93,153},{ 36,151,97,152},{ 36,152,101,152},{ 36,154,105,151},{ 36,156,110,151},{ 36,156,116,150},{ 36,158,121,150},{ 36,159,126,148},{ 36,161,131,148},{ 36,162,137,147},{ 36,164,142,147},{ 24,166,147,146, 12,167,147,146},{ 36,168,152,146},
		{ 36,154,84,151},{ 36,155,84,151},{ 36,155,86,151},{ 24,157,88,150, 12,156,88,150},{ 36,157,91,150},{ 36,158,94,149},{ 36,160,98,149},{ 36,161,103,148},{ 36,162,108,148},{ 36,163,112,147},{ 36,165,117,146},{ 36,166,123,146},{ 36,168,128,145},{ 36,170,133,145},{ 36,171,138,144},{ 36,173,144,143},{ 36,174,149,143},
		{ 36,161,82,148},{ 36,162,82,148},{ 36,162,83,148},{ 32,164,85,147, 4,163,85,147},{ 36,164,88,147},{ 36,165,91,147},{ 36,167,95,146},{ 36,168,99,145},{ 36,169,104,145},{ 36,170,109,144},{ 36,172,114,144},{ 36,173,119,143},{ 36,175,124,143},{ 36,177,129,142},{ 36,178,135,141},{ 36,180,140,141},{ 36,181,145,140},
		{ 36,169,80,145},{ 36,170,80,145},{ 36,170,81,145},{ 36,171,83,145},{ 36,172,85,144},{ 36,173,88,144},{ 36,174,92,143},{ 36,175,97,142},{ 36,177,101,142},{ 36,178,106,141},{ 36,179,111,141},{ 36,181,116,140},{ 36,182,121,139},{ 36,184,127,139},{ 28,185,132,138, 8,186,132,138},{ 36,187,137,138},{ 36,189,142,138},
		{ 36,176,78,142},{ 36,177,78,142},{ 36,177,79,142},{ 36,179,81,142},{ 36,180,83,141},{ 36,180,86,141},{ 36,182,90,140},{ 36,182,94,139},{ 36,184,98,139},{ 36,185,103,138},{ 36,187,108,138},{ 36,189,112,138},{ 36,190,118,137},{ 36,192,123,136},{ 36,192,128,136},{ 32,195,133,135, 4,194,133,135},{ 36,196,139,135},
		{ 36,184,75,140},{ 36,185,76,139},{ 36,186,76,139},{ 36,186,78,139},{ 36,187,80,138},{ 36,188,83,138},{ 36,189,86,138},{ 36,190,90,137},{ 36,192,95,136},{ 36,193,99,136},{ 36,194,104,135},{ 36,196,109,135},{ 36,197,114,134},{ 36,199,119,134},{ 36,200,124,133},{ 36,202,129,133},{ 24,203,135,132, 12,204,135,132},
		{ 36,191,73,137},{ 36,192,73,137},{ 28,193,74,136, 8,194,74,136},{ 36,194,75,136},{ 36,195,77,136},{ 36,195,81,135},{ 36,197,84,135},{ 32,198,88,134, 4,197,88,134},{ 36,199,92,134},{ 24,201,96,133, 12,202,96,133},{ 36,202,101,133},{ 36,204,106,132},{ 36,205,111,132},{ 36,207,116,131},{ 36,208,121,131},{ 36,210,126,130},{ 36,211,131,130},
		{ 36,199,71,134},{ 36,200,71,134},{ 36,200,71,134},{ 36,202,73,134},{ 36,202,75,134},{ 36,203,78,133},{ 36,204,81,132},{ 36,206,84,132},{ 36,207,88,132},{ 36,208,92,131},{ 36,210,97,131},{ 36,211,102,130},{ 36,213,107,129},{ 36,214,112,129},{ 36,216,117,128},{ 36,218,122,128},{ 36,219,128,128}
	};

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
