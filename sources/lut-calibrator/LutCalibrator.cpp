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
#endif

#define STRING_CSTR(x) (x.operator std::string()).c_str()

#include <QCoreApplication>

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
#include <linalg.h>
#include <fstream>


using namespace linalg;
using namespace aliases;
using namespace ColorSpaceMath;
using namespace BoardUtils;


#define LUT_FILE_SIZE 50331648
#define LUT_INDEX(y,u,v) ((y + (u<<8) + (v<<16))*3)
#define MAX_CALIBRATION_ERROR 10000000

struct BestResult{
	YuvConverter::YUV_COEFS coef = YuvConverter::YUV_COEFS::BT601;
	double4x4 coefMatrix;
	double2 coefDelta;
	double3 aspect;
	int bt2020Range = 0;
	int altConvert = 0;
	double3x3 altPrimariesToSrgb;
	ColorSpaceMath::HDR_GAMMA gamma = ColorSpaceMath::HDR_GAMMA::PQ;
	double gammaHLG = 0;
	double nits = 0;	

	struct Signal
	{
		YuvConverter::COLOR_RANGE range = YuvConverter::COLOR_RANGE::FULL;
		double yRange = 0, upYLimit = 0, downYLimit = 0, yShift = 0;
	} signal;

	long long int minError = MAX_CALIBRATION_ERROR;

	void serialize(std::stringstream& out)
	{
		out.precision(10);
		out << "/*" << std::endl;
		out << "BestResult bestResult;" << std::endl;
		out << "bestResult.coef = YuvConverter::YUV_COEFS(" << std::to_string(coef) << ");" << std::endl;
		out << "bestResult.coefMatrix = double4x4"; ColorSpaceMath::serialize(out, coefMatrix); out << ";" << std::endl;
		out << "bestResult.coefDelta = double2"; ColorSpaceMath::serialize(out, coefDelta); out << ";" << std::endl;
		out << "bestResult.aspect = double3";  ColorSpaceMath::serialize(out, aspect); out << ";" << std::endl;
		out << "bestResult.bt2020Range = " << std::to_string(bt2020Range) << ";" << std::endl;
		out << "bestResult.altConvert = " << std::to_string(altConvert) << ";" << std::endl;
		out << "bestResult.altPrimariesToSrgb = double3x3"; ColorSpaceMath::serialize(out, altPrimariesToSrgb); out << ";" << std::endl;
		out << "bestResult.gamma = ColorSpaceMath::HDR_GAMMA(" << std::to_string(gamma) << ");" << std::endl;
		out << "bestResult.gammaHLG = " << std::to_string(gammaHLG) << ";" << std::endl;
		out << "bestResult.nits = " << std::to_string(nits) << ";" << std::endl;
		out << "bestResult.signal.range = YuvConverter::COLOR_RANGE(" << std::to_string(signal.range) << ");" << std::endl;
		out << "bestResult.signal.yRange = " << std::to_string(signal.yRange) << ";" << std::endl;
		out << "bestResult.signal.upYLimit = " << std::to_string(signal.upYLimit) << ";" << std::endl;
		out << "bestResult.signal.downYLimit = " << std::to_string(signal.downYLimit) << ";" << std::endl;
		out << "bestResult.signal.yShift = " << std::to_string(signal.yShift) << ";" << std::endl;
		out << "bestResult.minError = " << std::to_string(minError) << ";" << std::endl;
		out << "*/" << std::endl;
	}
};

bool LutCalibrator::parseTextLut2binary(const char* filename, const char* outfile)
{
	std::ifstream stream(filename);
	if (!stream)
	{
		return false;
	}
	
	std::string dummy;
	double3 yuv;
	_lut.resize(256 * 256 * 256 * 3);
	memset(_lut.data(), 0, _lut.size());
	for (int R = 0; R < 256; R++)
		for (int G = 0; G < 256; G++)
			for (int B = 0; B < 256; B++)
				if (std::getline(stream, dummy, '[') >> yuv.x &&
					std::getline(stream, dummy, ',') >> yuv.y &&
					std::getline(stream, dummy, ',') >> yuv.z)
				{
					byte3 YUV = to_byte3(yuv);
					int index = LUT_INDEX(YUV.x, YUV.y, YUV.z);
					_lut.data()[index] = R;
					_lut.data()[index + 1] = G;
					_lut.data()[index + 2] = B;
					
				}
				else
					return false;

	for(int w = 0; w < 3; w++)
		for(int Y = 2; Y < 256; Y++)
			for (int U = 0; U < 256; U++)
				for (int V = 0; V < 256; V++)
				{
					int index = LUT_INDEX(Y, U, V);
					if (_lut.data()[index] == 0 && _lut.data()[index + 1] == 0 && _lut.data()[index + 2] == 0)
					{
						bool found = false;						
							for (int u = 0; u <= 2 && !found; u++)
								for (int v = 0; v <= 2 && !found; v++)
									for (int y = 0; y <= 2 && !found; y++)
								{
									int YY = Y + ((y % 2) ? y / 2 : -y / 2);
									int UU = U + ((u % 2) ? u / 2 : -u / 2);
									int VV = V + ((v % 2) ? v / 2 : -v / 2);
									if (YY >= 0 && YY <= 255 && UU >= 0 && UU <= 255 && VV >= 0 && VV <= 255)
									{
										int index2 = LUT_INDEX(YY, UU, VV);
										if (_lut.data()[index2] || _lut.data()[index2 + 1] || _lut.data()[index2 + 2])
										{
											found = true;
											_lut.data()[index] = _lut.data()[index2];
											_lut.data()[index + 1] = _lut.data()[index2 + 1];
											_lut.data()[index + 2] = _lut.data()[index2 + 2];
										}
									}
								}
					}
				}

	std::fstream file;
	file.open(outfile, std::ios::trunc | std::ios::out |  std::ios::binary);
	for(int i = 0; i < 3; i++)
		file.write(reinterpret_cast<char*>(_lut.data()), _lut.size());
	file.close();

	_lut.releaseMemory();
	return true;
};

LutCalibrator::LutCalibrator()
{
	_log = Logger::getInstance("CALIBRATOR");
	_capturedColors = std::make_shared<CapturedColors>();
	_yuvConverter = std::make_shared<YuvConverter>();
	_debug = true;
	_postprocessing = true;
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
	SignalLutCalibrationUpdated(report);
}

void LutCalibrator::notifyCalibrationMessage(QString message, bool started)
{
	QJsonObject report;
	report["message"] = message;
	if (started)
	{
		report["start"] = true;
	}
	SignalLutCalibrationUpdated(report);
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


void LutCalibrator::sendReport(QString report)
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

void LutCalibrator::startHandler(QString rootpath, hyperhdr::Components defaultComp, bool debug, bool postprocessing)
{
	_rootPath = rootpath;
	_debug = debug;
	_postprocessing = postprocessing;

	stopHandler();

	_capturedColors.reset();
	_capturedColors = std::make_shared<CapturedColors>();

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

	if (defaultComp == hyperhdr::COMP_VIDEOGRABBER)
	{
		auto message = "Using video grabber as a source<br/>Waiting for first captured test board..";
		notifyCalibrationMessage(message);
		Debug(_log, message);
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &LutCalibrator::setVideoImage, Qt::ConnectionType::UniqueConnection);
	}
	else if (defaultComp == hyperhdr::COMP_SYSTEMGRABBER)
	{
		auto message = "Using system grabber as a source<br/>Waiting for first captured test board..";
		notifyCalibrationMessage(message);
		Debug(_log, message);
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &LutCalibrator::setSystemImage, Qt::ConnectionType::UniqueConnection);
	}
	else
	{
		auto message = "Using flatbuffers/protobuffers as a source<br/>Waiting for first captured test board..";
		notifyCalibrationMessage(message);
		Debug(_log, message);
		connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &LutCalibrator::signalSetGlobalImageHandler, Qt::ConnectionType::UniqueConnection);
	}
}

void LutCalibrator::stopHandler()
{
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage, this, &LutCalibrator::setSystemImage);
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &LutCalibrator::setVideoImage);
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &LutCalibrator::signalSetGlobalImageHandler);
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
	if (pixelFormat != PixelFormat::NV12 && pixelFormat != PixelFormat::MJPEG && pixelFormat != PixelFormat::YUYV)
	{
		error("Only NV12/MJPEG/YUYV video format for the USB grabber and NV12 for the flatbuffers source are supported for the LUT calibration.");
		return;
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
	byte3 prime;
	double3 org;
	double3 real;
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

void doToneMapping(std::list<MappingPrime>& m, double3& p)
{
	auto a = xyz_to_lch(from_sRGB_to_XYZ(p) * 100.0);
	auto iter = m.begin();
	auto last = *(iter++);
	for (; iter != m.end(); last = *(iter++))
		if ((last.real.z >= a.z && a.z >= (*iter).real.z))
		{
			auto& current = (*iter);
			double lastAsp = last.real.z - a.z;
			double curAsp = a.z - current.real.z;
			double prop = 1 - (lastAsp / (lastAsp + curAsp));
			double chromaLastAsp = clamp(a.y / last.real.y, 0.0, 1.0);
			double chromaCurrentAsp = clamp(a.y / current.real.y, 0.0, 1.0);
			a.y += prop * last.delta.y * chromaLastAsp + (1 - prop) * current.delta.y * chromaCurrentAsp;
			a.z += prop * last.delta.z + (1 - prop) * current.delta.z;
			
			p = from_XYZ_to_sRGB(lch_to_xyz(a) / 100.0);
			return;
		}	
}

void LutCalibrator::printReport()
{
	QStringList info, intro;
	const int SCALE = SCREEN_COLOR_DIMENSION - 1;
	std::list<MappingPrime> m = {
		{ /* GREEN       */ {0       ,SCALE   ,0       }, {}, {} },
		{ /* GREEN_BLUE  */ {0       ,SCALE   ,SCALE   }, {}, {} },
		{ /* BLUE        */ {0       ,0       ,SCALE   }, {}, {} },
		{ /* RED_BLUE    */ {SCALE   ,0       ,SCALE   }, {}, {} },
		{ /* RED         */ {SCALE   ,0       ,0       }, {}, {} },
		{ /* RED_GREEN   */ {SCALE   ,SCALE   ,0       }, {}, {} },
		{ /* RED_GREEN2  */ {SCALE   ,SCALE/2 ,0       }, {}, {} },
		{ /* GREEN_BLUE2 */ {0       ,SCALE   ,SCALE/2 }, {}, {} },
		{ /* RED_BLUE2   */ {SCALE   ,0       ,SCALE/2 }, {}, {} },
		{ /* RED2_GREEN  */ {SCALE/2 ,SCALE   ,0       }, {}, {} },
		{ /* GREEN2_BLUE */ {0       ,SCALE/2 ,SCALE   }, {}, {} },
		{ /* RED2_BLUE   */ {SCALE/2 ,0       ,SCALE   }, {}, {} },
	};
	/*
	for (auto& c : m)
	{
		auto sample = _capturedColors->all[c.prime.x][c.prime.y][c.prime.z];
		auto a = to_double3(sample.getSourceRGB()) / 255.0;
		c.org = xyz_to_lch(from_sRGB_to_XYZ(a) * 100.0);		


		auto b = to_double3(sample.getFinalRGB()) / 255.0;
		c.real = xyz_to_lch(from_sRGB_to_XYZ(b) * 100.0);
		c.delta = c.org - c.real;
	}
	m.sort([](const MappingPrime& a, const MappingPrime& b) { return a.real.z > b.real.z; });

	auto loopEnd = m.front();
	auto loopFront = m.back();

	loopEnd.org.z -= 360;
	loopEnd.real.z -= 360;
	m.push_back(loopEnd);

	loopFront.org.z += 360;
	loopFront.real.z += 360;
	m.push_front(loopFront);

	info.append("Primaries in LCH colorspace");
	info.append("name,      RGB primary in LCH,     captured primary in LCH       |  primary RGB  |   average LCH delta       |  LCH to RGB way back ");
	info.append("--------------------------------------------------------------------------------------------------------------------------------------------------------");
	for (const auto& c : m)
	{
		auto sample = _capturedColors->all[c.prime.x][c.prime.y][c.prime.z];
		auto aa = from_XYZ_to_sRGB(lch_to_xyz(c.org) / 100.0) * 255;
		auto bb = from_XYZ_to_sRGB(lch_to_xyz(c.real) / 100.0) * 255;
		info.append(QString("%1 %2 %3 | %4 %5 | %6 | %7").arg(vecToString(sample.getSourceRGB()), 12).
											arg(vecToString(c.org)).
											arg(vecToString(c.real)).
											arg(vecToString(sample.getSourceRGB()), 12).
											arg(vecToString(c.delta)).
											arg(vecToString(to_byte3(aa))).
											arg(vecToString(to_byte3(bb))));
											
	}

	
	info.append("--------------------------------------------------------------------------------------------------------------------------------------------------------");
	info.append("");
	info.append("");
	info.append("                                 LCH mapping correction");
	info.append("         Source sRGB color => captured => Rec.2020 processing => LCH final correction");
	info.append("-------------------------------------------------------------------------------------------------");
	*/
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
	sendReport(info.join("\r\n"));
}


double3 hdr_to_srgb(YuvConverter* _yuvConverter, double3 yuv, const linalg::vec<uint8_t, 2>& UV, const double3& aspect, const double4x4& coefMatrix, ColorSpaceMath::HDR_GAMMA gamma, double gammaHLG, double nits, int altConvert, const double3x3& bt2020_to_sRgb, int tryBt2020Range, const BestResult::Signal& signal)
{
	
	double3 srgb;

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
	if (UV.x != UV.y || UV.x < 127 || UV.x > 129)
	{
		const double mid = 128.0 / 256.0;
		yuv.y = (yuv.y - mid) * aspect.y + mid;
		yuv.z = (yuv.z - mid) * aspect.z + mid;
	}

	auto a = _yuvConverter->multiplyColorMatrix(coefMatrix, yuv);

	double3 e;

	if (gamma == HDR_GAMMA::PQ)
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

	ColorSpaceMath::trim01(srgb);

	if (tryBt2020Range)
	{
		srgb = bt2020_nonlinear_to_linear(srgb);
		srgb = srgb_linear_to_nonlinear(srgb);
	}

	return srgb;
}

void  LutCalibrator::fineTune()
{
	const auto MAX_IND = SCREEN_COLOR_DIMENSION - 1;
	const auto white = _capturedColors->all[MAX_IND][MAX_IND][MAX_IND].Y();
	double4 nits{ 0, 0, 0, 0 };
	double maxLevel = 0;

	bestResult->signal.range = _capturedColors->getRange();

	_capturedColors->getSignalParams(bestResult->signal.yRange, bestResult->signal.upYLimit, bestResult->signal.downYLimit, bestResult->signal.yShift);

	if (bestResult->signal.range == YuvConverter::COLOR_RANGE::LIMITED)
	{
		maxLevel = (white - 16.0) / (235.0 - 16.0);
	}
	else
	{
		maxLevel = white / 255.0;
	}

	nits[HDR_GAMMA::PQ] = 10000.0 * PQ_ST2084(1.0, maxLevel);

	for (int gamma = HDR_GAMMA::PQ; gamma <= HDR_GAMMA::BT2020inSRGB; gamma++)
	{
		std::vector<double> gammasHLG;
			
		if (gamma == HDR_GAMMA::HLG)
			gammasHLG = { 0 , 1.2, 1.1};
		else
			gammasHLG = { 0 };

		for (double gammaHLG : gammasHLG)
		{
			if (gammaHLG == 1.1 && bestResult->gamma != HDR_GAMMA::HLG)
				break;

			if (gamma == HDR_GAMMA::HLG)
			{
				nits[HDR_GAMMA::HLG] = 1 / OOTF_HLG(inverse_OETF_HLG(maxLevel), gammaHLG).x;
			}
			for (int coef = YuvConverter::YUV_COEFS::BT601; coef <= YuvConverter::YUV_COEFS::BT2020; coef++)
			{
				double3x3 convert_bt2020_to_XYZ;
				double3x3 convert_XYZ_to_sRgb;

				capturedPrimariesCorrection(HDR_GAMMA(gamma), gammaHLG, nits[gamma], coef, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb);
				auto bt2020_to_sRgb = mul(convert_XYZ_to_sRgb, convert_bt2020_to_XYZ);

				printf("Processing gamma: %s, gammaHLG: %f, coef: %s. Current best gamma: %s, gammaHLG: %f, coef: %s. Score: %lli\n",
					QSTRING_CSTR(gammaToString(HDR_GAMMA(gamma))), gammaHLG, QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef))),
					QSTRING_CSTR(gammaToString(HDR_GAMMA(bestResult->gamma))), bestResult->gammaHLG, QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(bestResult->coef))), bestResult->minError / 1000);

				const int halfKDelta = 9;
				for (int krIndex = 0; krIndex <= 2 * halfKDelta; krIndex += (_postprocessing) ? 1 : 4 * halfKDelta)
					for (int kbIndex = 0; kbIndex <= 2 * halfKDelta; kbIndex += (_postprocessing) ? 1 : 4 * halfKDelta)
					{
						double2 kDelta = double2(((krIndex <= halfKDelta) ? -krIndex : krIndex - halfKDelta),
							((kbIndex <= halfKDelta) ? -kbIndex : kbIndex - halfKDelta)) * 0.002;

						auto coefValues = _yuvConverter->getCoef(YuvConverter::YUV_COEFS(coef)) + kDelta;
						auto coefMatrix = _yuvConverter->create_yuv_to_rgb_matrix(bestResult->signal.range, coefValues.x, coefValues.y);

						for (int altConvert = 0; altConvert <= 1; altConvert++)
							for (int tryBt2020Range = 0; tryBt2020Range <= 1; tryBt2020Range++)
								for (double aspectX = 1.015; aspectX >= 0.9999; aspectX -= (_postprocessing) ? 0.0025 : aspectX)
									for (double aspectYZ = 1.0; aspectYZ <= 1.1301; aspectYZ += (_postprocessing) ? 0.005 : aspectYZ)

									{
										double3 aspect(aspectX, aspectYZ, aspectYZ);

										long long int currentError = 0;
										for (int rr = MAX_IND; rr >= 0 && currentError < bestResult->minError; rr--)
											for (int gg = MAX_IND; gg >= 0 && currentError < bestResult->minError; gg--)
												for (int bb = MAX_IND; bb >= 0 && currentError < bestResult->minError; bb--)
												{
													int r = (rr == MAX_IND) ? MAX_IND - 1 : ((rr == MAX_IND - 1) ? MAX_IND : rr);
													int g = (gg == MAX_IND) ? MAX_IND - 1 : ((gg == MAX_IND - 1) ? MAX_IND : gg);
													int b = (bb == MAX_IND) ? MAX_IND - 1 : ((bb == MAX_IND - 1) ? MAX_IND : bb);

													if ((r % 2 == 0 && g % 4 == 0 && b % 4 == 0) || (r == b && b == g))
													{
														auto minError = MAX_CALIBRATION_ERROR;

														const auto& sample = _capturedColors->all[r][g][b];
														auto sampleList = sample.getInputYuvColors();

														for (auto iter = sampleList.cbegin(); iter != sampleList.cend(); ++iter)
														{
															auto srgb = hdr_to_srgb(_yuvConverter.get(), (*iter).first, byte2{ sample.U(), sample.V() }, aspect, coefMatrix, HDR_GAMMA(gamma), gammaHLG, nits[gamma], altConvert, bt2020_to_sRgb, tryBt2020Range, bestResult->signal);
															auto SRGB = to_int3(srgb * 255.0);

															auto sampleError = sample.getSourceError(SRGB);															

															if ((r + 2 == SCREEN_COLOR_DIMENSION && g + 2 == SCREEN_COLOR_DIMENSION && b + 2 == SCREEN_COLOR_DIMENSION &&
																(SRGB.x > 248 || SRGB.x < 232)))
																sampleError = bestResult->minError;

															if (sampleError < minError)
																minError = sampleError;
														}

														currentError += minError;

														
													}
												}
										if (currentError < bestResult->minError)
										{
											bestResult->minError = currentError;
											bestResult->coef = YuvConverter::YUV_COEFS(coef);
											bestResult->coefDelta = kDelta;
											bestResult->bt2020Range = tryBt2020Range;											
											bestResult->altConvert = altConvert;
											bestResult->altPrimariesToSrgb = bt2020_to_sRgb;
											bestResult->aspect = aspect;
											bestResult->nits = nits[gamma];
											bestResult->gamma = HDR_GAMMA(gamma);
											bestResult->gammaHLG = gammaHLG;
											bestResult->coefMatrix = coefMatrix;
										}
									}
					}
			}
		}
	}
}


void LutCalibrator::calibration()
{
	// calibration
	auto totalTime = InternalClock::now();
	fineTune();
	totalTime = InternalClock::now() - totalTime;

	if (bestResult->minError >= MAX_CALIBRATION_ERROR)
	{
		error("The calibration failed. The error is too high.");
		return;
	}
	
	// write result
	Debug(_log, "Score: %f", bestResult->minError / 1000.0);
	Debug(_log, "Time: %f", totalTime / 1000.0);
	Debug(_log, "The post-processing is: %s", (_postprocessing) ? "enabled" : "disabled");
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
	Debug(_log, "Selected bt2020 gamma range: %i", bestResult->bt2020Range);
	Debug(_log, "Selected alternative conversion of primaries: %i", bestResult->altConvert);
	Debug(_log, "Selected aspect: %f %f %f", bestResult->aspect.x, bestResult->aspect.y, bestResult->aspect.z);

	if (_debug)
	{
		double3x3 convert_bt2020_to_XYZ;
		double3x3 convert_XYZ_to_sRgb;

		capturedPrimariesCorrection(HDR_GAMMA(bestResult->gamma), bestResult->gammaHLG, bestResult->nits, bestResult->coef, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb, true);
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

	_lut.releaseMemory();
	QString errorMessage = writeLUT(_log, _rootPath, bestResult.get(), &(_capturedColors->all));
	if (!errorMessage.isEmpty())
	{
		error(errorMessage);
	}

	// reload LUT
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, false);
	QThread::msleep(500);
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);
}

QString LutCalibrator::writeLUT(Logger* _log, QString _rootPath, BestResult* bestResult, std::vector<std::vector<std::vector<CapturedColor>>>* all)
{
	// write LUT table
	QString fileName = QString("%1%2").arg(_rootPath).arg("/lut_lin_tables.3d");
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

		Info(_log, "Writing LUT file to: %s", QSTRING_CSTR(fileName));
		_lut.resize(LUT_FILE_SIZE);
		for (int phase = 0; phase < 3; phase++)
		{
			for (int y = 0; y <= 255; y++)
				for (int u = 0; u <= 255; u++)
					for (int v = 0; v <= 255; v++)
					{
						byte3 YUV(y, u, v);
						double3 yuv = to_double3(YUV) / 255.0;

						if (phase == 0)
						{
							yuv = yuvConverter.toYuv(bestResult->signal.range, bestResult->coef, yuv);
							YUV = to_byte3(yuv * 255);
						}

						if (phase == 0 || phase == 1)
						{
							if (YUV.y >= 127 && YUV.y <= 129 && YUV.z >= 127 && YUV.z <= 129)
							{
								YUV.y = 128;
								YUV.z = 128;
								yuv.y = 128.0 / 255.0;
								yuv.z = 128.0 / 255.0;
							}
							yuv = hdr_to_srgb(&yuvConverter, yuv, byte2(YUV.y, YUV.z), bestResult->aspect, bestResult->coefMatrix, bestResult->gamma, bestResult->gammaHLG, bestResult->nits, bestResult->altConvert, bestResult->altPrimariesToSrgb, bestResult->bt2020Range, bestResult->signal);
						}
						else
						{
							yuv = yuvConverter.toRgb(bestResult->signal.range, bestResult->coef, yuv);
						}

						byte3 result = to_byte3(yuv * 255.0);
						uint32_t ind_lutd = LUT_INDEX(y, u, v);
						_lut.data()[ind_lutd] = result.x;
						_lut.data()[ind_lutd + 1] = result.y;
						_lut.data()[ind_lutd + 2] = result.z;
					}

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
		file.close();
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
		sendReport(_yuvConverter->toString());
	#endif

	bestResult = std::make_shared<BestResult>();
	_capturedColors->finilizeBoard();


	sendReport("Captured colors:\r\n" +
				generateReport(false));

	calibration();

	sendReport("Calibrated:\r\n" +
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
		auto a = _yuvConverter->toRgb(_capturedColors->getRange(), YuvConverter::YUV_COEFS(coef), c.yuv());

		if (gamma == ColorSpaceMath::HDR_GAMMA::PQ)
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
		Debug(_log, "--------------------------------- Actual PQ primaries for YUV coefs: %s ---------------------------------", QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef))));
		Debug(_log, "r: (%.3f, %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f) vs wide(%.3f, %.3f)", sRgb_red_xy.x, sRgb_red_xy.y, 0.64f, 0.33f, 0.708f, 0.292f, 0.7350f, 0.2650f);
		Debug(_log, "g: (%.3f, %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f) vs wide(%.3f, %.3f)", sRgb_green_xy.x, sRgb_green_xy.y, 0.30f, 0.60f, 0.17f, 0.797f, 0.1150f, 0.8260f);
		Debug(_log, "b: (%.3f, %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f) vs wide(%.3f, %.3f)", sRgb_blue_xy.x, sRgb_blue_xy.y, 0.15f, 0.06f, 0.131f, 0.046f, 0.1570f, 0.0180f);
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

				for (int i = 0; i < colors.size(); i += 4)
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

