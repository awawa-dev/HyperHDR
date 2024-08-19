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
#include <linalg.h>
#include <utils-image/utils-image.h>


using namespace linalg;
using namespace aliases;
using namespace ColorSpaceMath;

double3 tovec(LutCalibrator::ColorStat a)
{
	return double3(a.red, a.green, a.blue);
}

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


namespace
{
	const int SCREEN_BLOCKS_X = 40;
	const int SCREEN_BLOCKS_Y = 24;
	const int SCREEN_COLOR_STEP = 16;
	const int SCREEN_COLOR_DIMENSION = (256 / SCREEN_COLOR_STEP);
}

static int indexToColorAndPos(int index, ColorRgb& color, int2& position)
{	
	int currentIndex = 0;
	int boardIndex = 0;

	position = int2((boardIndex + 1) % 2, 1);

	for (int R = 0; R <= 256; R += ((R == 0) ? 2 * SCREEN_COLOR_STEP : SCREEN_COLOR_STEP))
		for (int G = 0; G <= 256; G += ((G == 0) ? 2 * SCREEN_COLOR_STEP : SCREEN_COLOR_STEP))
			for (int B = 0; B <= 256; B += ((B == 0) ? 2 * SCREEN_COLOR_STEP : SCREEN_COLOR_STEP), currentIndex++)
				if (index == currentIndex)
				{
					color.red = std::min(R, 255);
					color.green = std::min(G, 255);
					color.blue = std::min(B, 255);
					return boardIndex;
				}
				else
				{
					position.x += 2;
					if (position.x >= SCREEN_BLOCKS_X)
					{						
						position.x = (++(position.y) + boardIndex) % 2;
					}
					if (position.y >= SCREEN_BLOCKS_Y - 1)
					{
						boardIndex++;
						position = int2((boardIndex + 1) % 2, 1);
					}
				}
	return -1;
}
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,colorspace=space=bt709:primaries=bt709:range=pc:trc=iec61966-2-1:ispace=bt709:iprimaries=bt709:irange=pc:itrc=iec61966-2-1:format=yuv444p10:fast=0:dither=none -c:v libx265 -pix_fmt yuv444p10le -profile:v main444-10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:lossless=1:colorprim=bt709:transfer=iec61966-2-1:colormatrix=bt709:range=full -f mp4 test_SDR_yuv444_high_quality.mp4
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,zscale=m=bt2020nc:p=bt2020:t=smpte2084:r=full:min=709:pin=709:tin=iec61966-2-1:rin=full:c=topleft,format=yuv444p10 -c:v libx265 -vtag hvc1 -pix_fmt yuv444p10le -profile:v main444-10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:hdr10=1:lossless=1:colorprim=bt2020:transfer=bt2020-10:colormatrix=bt2020nc:range=full -f mp4 test_HDR_yuv444_high_quality.mp4

// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,colorspace=space=bt709:primaries=bt709:range=pc:trc=iec61966-2-1:ispace=bt709:iprimaries=bt709:irange=pc:itrc=iec61966-2-1:format=yuv444p12:fast=0:dither=none -c:v libx265 -pix_fmt yuv420p10le -profile:v main10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:lossless=1:colorprim=bt709:transfer=iec61966-2-1:colormatrix=bt709:range=full -f mp4 test_SDR_yuv420_low_quality.mp4
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,zscale=m=bt2020nc:p=bt2020:t=smpte2084:r=full:min=709:pin=709:tin=iec61966-2-1:rin=full:c=topleft,format=yuv444p12 -c:v libx265 -vtag hvc1 -pix_fmt yuv420p10le -profile:v main10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:hdr10=1:lossless=1:colorprim=bt2020:transfer=bt2020-10:colormatrix=bt2020nc:range=full -f mp4 test_HDR_yuv420_low_quality.mp4

LutCalibrator::ColorStat readBlock(const Image<ColorRgb>& image, int2 position)
{
	const int2 delta (image.width() / SCREEN_BLOCKS_X, image.height() / SCREEN_BLOCKS_Y);
	LutCalibrator::ColorStat color;

	const int2 start = position * delta;
	const int2 end = ((position + int2(1, 1)) * delta) - int2(1, 1);
	const int2 middle = (start + end) / 2;
	return color;
}

static void detectBoard(const Image<ColorRgb>& image)
{
	const int dX = image.width() / SCREEN_BLOCKS_X;
	const int dY = image.height() / SCREEN_BLOCKS_Y;
}

static void createTestBoards()
{
	constexpr int2 margin(3, 2);
	int maxIndex = std::pow(SCREEN_COLOR_DIMENSION, 3);
	int boardIndex = 0;
	Image<ColorRgb> image(1920, 1080);
	const int2 delta(image.width() / SCREEN_BLOCKS_X, image.height() / SCREEN_BLOCKS_Y);

	auto saveImage = [](Image<ColorRgb> &image, const int2& delta, int boardIndex)
	{		
		for (int line = 0; line < SCREEN_BLOCKS_Y; line += SCREEN_BLOCKS_Y - 1)
		{
			int2 position = int2((line + boardIndex) % 2, line);

			for (int x = 0; x < boardIndex + 5; x++, position.x += 2)
			{
				const int2 start = position * delta;
				const int2 end = ((position + int2(1, 1)) * delta) - int2(1, 1);
				image.fastBox(start.x + margin.x, start.y + margin.y, end.x - margin.x, end.y - margin.y, 255, 255, 255);
			}
		}
		utils_image::savePng(QString("D:/table_%1.png").arg(QString::number(boardIndex)).toStdString(), image);
	};


	image.clear();

	if (256 % SCREEN_COLOR_STEP > 0 || image.width() % SCREEN_BLOCKS_X || image.height() % SCREEN_BLOCKS_Y)
		return;

	for(int index = 0; index < maxIndex; index++)
	{
		ColorRgb color;
		int2 position;
		int currentBoard = indexToColorAndPos(index, color, position);

		if (currentBoard < 0)
			return;

		if (boardIndex != currentBoard)
		{
			saveImage(image, delta, boardIndex);
			image.clear();
			boardIndex = currentBoard;
		}

		const int2 start = position * delta;
		const int2 end = ((position + int2(1, 1)) * delta) - int2(1, 1);

		image.fastBox(start.x + margin.x, start.y + margin.y, end.x - margin.x, end.y - margin.y, color.red, color.green, color.blue);
	}
	saveImage(image, delta, boardIndex);
}

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

void LutCalibrator::error(QString message)
{
	QJsonObject report;
	stopHandler();
	Error(_log, QSTRING_CSTR(message));
	report["status"] = 1;
	report["error"] = message;
	SignalLutCalibrationUpdated(report);
}

struct UserConfig {

	double	saturation;
	double	luminance;
	double	gammaR;
	double	gammaG;
	double	gammaB;

	UserConfig() : UserConfig(0, 0, 0, 0, 0) {};
	UserConfig(double _saturation, double _luminance, double _gammaR, double _gammaG, double _gammaB):
		saturation(_saturation),
		luminance(_luminance),
		gammaR(_gammaR),
		gammaG(_gammaG),
		gammaB(_gammaB)
	{}

} userConfig;

enum TEST_COLOR_ID {
	WHITE = 0,
	RED = 1, GREEN = 2, BLUE = 3,
	RED_GREEN = 4, GREEN_BLUE = 5, RED_BLUE = 6,
	RED_GREEN2 = 7, GREEN_BLUE2 = 8, RED_BLUE2 = 9,
	RED2_GREEN = 10, GREEN2_BLUE = 11, RED2_BLUE = 12
};

namespace {
	const int LOGIC_BLOCKS_X_SIZE = 4;
	const int LOGIC_BLOCKS_X = std::floor((SCREEN_BLOCKS_X - 1) / LOGIC_BLOCKS_X_SIZE);
	const int COLOR_DIVIDES = 32;	

	const std::map<TEST_COLOR_ID, std::pair<byte3, QString>> TEST_COLORS = {
		{TEST_COLOR_ID::WHITE,      {{255, 255, 255}, "White"}},
		{TEST_COLOR_ID::RED,        {{255, 0,   0  }, "Red"}},
		{TEST_COLOR_ID::GREEN,      {{0,   255, 0  }, "Green"}},
		{TEST_COLOR_ID::BLUE,       {{0,   0,   255}, "Blue"}},
		{TEST_COLOR_ID::RED_GREEN,  {{255, 255, 0  }, "Yellow"}},
		{TEST_COLOR_ID::GREEN_BLUE, {{0,   255, 255}, "Cyan"}},
		{TEST_COLOR_ID::RED_BLUE,   {{255,   0, 255}, "Magenta"}},
		{TEST_COLOR_ID::RED_GREEN2, {{255, 128, 0  }, "Orange"}},
		{TEST_COLOR_ID::GREEN_BLUE2,{{0,   255, 128}, "LimeBlue"}},
		{TEST_COLOR_ID::RED_BLUE2,  {{255,   0, 128}, "Pink"}},
		{TEST_COLOR_ID::RED2_GREEN, {{128, 255, 0  }, "LimeRed"}},
		{TEST_COLOR_ID::GREEN2_BLUE,{{0,   128, 255}, "Azure"}},
		{TEST_COLOR_ID::RED2_BLUE,  {{128,   0, 255}, "Violet"}}
	};


	enum CALIBRATION_STEP {
		NOT_ACTIVE = 0, STEP_1_RAW_YUV, STEP_2_CALIBRATE
	};

	YuvConverter yuvConverter;
}

void LutCalibrator::applyShadow(linalg::vec<double,3>& color, int shadow)
{
	color.x -= std::round(shadow * (color.x + ((int)color.x % 2)) / COLOR_DIVIDES) - ((shadow == 0) ? 0 : ((int)color.x % 2));
	color.y -= std::round(shadow * (color.y + ((int)color.y % 2)) / COLOR_DIVIDES) - ((shadow == 0) ? 0 : ((int)color.y % 2));
	color.z -= std::round(shadow * (color.z + ((int)color.z % 2)) / COLOR_DIVIDES) - ((shadow == 0) ? 0 : ((int)color.z % 2));
}

bool LutCalibrator::getSourceColor(int index, linalg::vec<double,3>& color, TEST_COLOR_ID& prime, int& shadow)
{
	int searching = 0;
	
	for (auto const& testColor : TEST_COLORS)
	{
		const byte3& refColor = testColor.second.first;
		color = double3(refColor.x, refColor.y, refColor.z);
		for (prime = testColor.first, shadow = 0; shadow < COLOR_DIVIDES; shadow++)
		{
			if (searching == index)
			{
				applyShadow(color, shadow);
				return true;
			}
			else
				searching++;
		}
	}

	return false;
}

struct Result
{
	/// color that was rendered by the browser
	double3 source = { 0, 0, 0 };
	/// color that was rendered by the browser RGB [0 - 1]
	double3 sourceScaled = { 0, 0, 0 };
	/// color that was captured RGB or YUV [0-255]
	double3 input = { 0, 0, 0 };
	/// color that was captured in limited YUV [0-235-240]
	double3 inputYUV = { 0, 0, 0 };
	/// converted to linear RGB
	double3 inputRGB = { 0, 0, 0 };
	/// input to XYZ
	double3 processingXYZ = { 0, 0, 0 };
	/// output RGB not rounded [0-1]
	double3 output = { 0, 0, 0 };
	/// output scaled RGB not rounded [0-1]
	double3 outputNormRGB = { 0, 0, 0 };
	/// output RGB not rounded [0 - 255 scale]
	double3 outputRGB = { 0, 0, 0 };

};

struct Calibration {
	CALIBRATION_STEP step;
	bool isYUV;	
	bool isVideoCapturingEnabled;
	double nits;
	YuvConverter::COLOR_RANGE colorRange;
	std::map<TEST_COLOR_ID, std::vector<Result>> results;
	std::map<YuvConverter::YUV_COEFS, double3x3> whitePointCorrection;

	Calibration() : Calibration(CALIBRATION_STEP::NOT_ACTIVE, false) {};
	Calibration(CALIBRATION_STEP _step, bool _isYUV) :
		step(_step),
		isYUV(_isYUV),
		isVideoCapturingEnabled(false),
		nits(0),
		colorRange(YuvConverter::COLOR_RANGE::FULL)
	{
		for(const auto& i : TEST_COLORS)
			results[i.first] = std::vector<Result>(COLOR_DIVIDES);
	}
} calibration;

double getVecMax(const double3& rec)
{
	return ((calibration.isYUV) ? rec.x : maxelem(rec));
}

double getVecMin(const double3& rec)
{
	return ((calibration.isYUV) ? rec.x : minelem(rec));
}

/*
WHITE = 0,
RED = 1, GREEN = 2, BLUE = 3,
RED_GREEN = 4, GREEN_BLUE = 5, RED_BLUE = 6,
RED_GREEN2 = 7, GREEN_BLUE2 = 8, RED_BLUE2 = 9,
GREEN_RED2 = 10, BLUE_GREEN2 = 11, BLUE_RED2 = 12


ColorRgb LutCalibrator::primeColors[] = {
				ColorRgb(255, 0, 0),
				ColorRgb(0, 255, 0),
				ColorRgb(0, 0, 255),
				ColorRgb(255, 255, 0),
				ColorRgb(255, 0, 255),
				ColorRgb(0, 255, 255),
				ColorRgb(255, 128, 0),
				ColorRgb(255, 0, 128),
				ColorRgb(0, 128, 255),
				ColorRgb(128, 64, 0),
				ColorRgb(128, 0, 64),
				ColorRgb(128, 0, 0),
				ColorRgb(0, 128, 0),
				ColorRgb(0, 0, 128),
				ColorRgb(16, 16, 16),
				ColorRgb(32, 32, 32),
				ColorRgb(48, 48, 48),
				ColorRgb(64, 64, 64),
				ColorRgb(96, 96, 96),
				ColorRgb(120, 120, 120),
				ColorRgb(144, 144, 144),
				ColorRgb(172, 172, 172),
				ColorRgb(196, 196, 196),
				ColorRgb(220, 220, 220),
				ColorRgb(255, 255, 255),
				ColorRgb(0, 0, 0)
};
*/
QString LutCalibrator::generateShortReport(std::function<QString(const Result&)> selector)
{
	const std::list<std::pair<TEST_COLOR_ID, int>> shortReportColors = {
			{ TEST_COLOR_ID::WHITE,      0 },
			{ TEST_COLOR_ID::RED,        0 },
			{ TEST_COLOR_ID::GREEN,      0 },
			{ TEST_COLOR_ID::BLUE,       0 },
			{ TEST_COLOR_ID::RED,        COLOR_DIVIDES / 2 },
			{ TEST_COLOR_ID::GREEN,      COLOR_DIVIDES / 2 },
			{ TEST_COLOR_ID::BLUE,       COLOR_DIVIDES / 2 },
			{ TEST_COLOR_ID::RED_GREEN,  0 },
			{ TEST_COLOR_ID::RED_BLUE,   0 },
			{ TEST_COLOR_ID::GREEN_BLUE, 0 },
			{ TEST_COLOR_ID::RED_GREEN2, 0 },
			{ TEST_COLOR_ID::GREEN_BLUE2,0 },
			{ TEST_COLOR_ID::RED_BLUE2,  0 },
			{ TEST_COLOR_ID::RED2_GREEN, 0 },
			{ TEST_COLOR_ID::GREEN2_BLUE,0 },
			{ TEST_COLOR_ID::RED2_BLUE,  0 },
			{ TEST_COLOR_ID::WHITE,      0  },
			{ TEST_COLOR_ID::WHITE,      1  },
			{ TEST_COLOR_ID::WHITE,      2  },
			{ TEST_COLOR_ID::WHITE,      5  },
			{ TEST_COLOR_ID::WHITE,      9 },
			{ TEST_COLOR_ID::WHITE,      12 },
			{ TEST_COLOR_ID::WHITE,      15 },
			{ TEST_COLOR_ID::WHITE,      18 },
			{ TEST_COLOR_ID::WHITE,      21 },
			{ TEST_COLOR_ID::WHITE,      24 },
			{ TEST_COLOR_ID::WHITE,      26 },
			{ TEST_COLOR_ID::WHITE,      29 },
			{ TEST_COLOR_ID::WHITE,      30 },
			{ TEST_COLOR_ID::WHITE,      COLOR_DIVIDES - 1 }
	};

	QStringList rep;
	for (const auto& color : shortReportColors)
		if (color.second >= 0 && color.second < COLOR_DIVIDES)
		{
			const auto& testColor = TEST_COLORS.at(color.first);
			vec<double, 3> sourceColor(testColor.first.x, testColor.first.y, testColor.first.z);
			applyShadow(sourceColor, color.second);
			rep.append(QString("%1: %2 => %3")
				.arg(QString("%1").arg(testColor.second + ((color.second != 0)? QString::number(color.second) : "")), 12)
				.arg(vecToString(to_byte3(sourceColor)), 12)
				.arg(selector(calibration.results[color.first][color.second])));

		};
	return rep.join("\r\n");
}

void LutCalibrator::requestNextTestBoard(int nextStep)
{	
	QJsonObject report;
	report["status"] = 0;
	report["validate"] = nextStep;
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
		QThread::msleep(100);

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

void LutCalibrator::incomingCommand(QString rootpath, GrabberWrapper* grabberWrapper, hyperhdr::Components defaultComp, int checksum, double saturation, double luminance, double gammaR, double gammaG, double gammaB)
{
	_rootPath = rootpath;

	if (checksum == CALIBRATION_STEP::NOT_ACTIVE)
	{
		stopHandler();

		bool isYuv = false;

		// check if the source is using YUV
		if (grabberWrapper != nullptr)
		{
			QString vidMode;
			SAFE_CALL_0_RET(grabberWrapper, getVideoCurrentModeResolution, QString, vidMode);
			isYuv = (vidMode.indexOf(pixelFormatToString(PixelFormat::MJPEG), 0, Qt::CaseInsensitive) >= 0) ||
					(vidMode.indexOf(pixelFormatToString(PixelFormat::YUYV), 0, Qt::CaseInsensitive) >= 0) ||
					(vidMode.indexOf(pixelFormatToString(PixelFormat::NV12), 0, Qt::CaseInsensitive) >= 0) ||
					(vidMode.indexOf(pixelFormatToString(PixelFormat::I420), 0, Qt::CaseInsensitive) >= 0);
			if (isYuv)
			{
				emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, true);

				int hdrEnabled = 0;
				SAFE_CALL_0_RET(grabberWrapper, getHdrToneMappingEnabled, int, hdrEnabled);
				if (!hdrEnabled)
				{
					error("Unexpected HDR state. Aborting");
					return;
				}
			}
		}

		if (!isYuv)
			emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, false);		

		userConfig= UserConfig(saturation, luminance, gammaR, gammaG, gammaB);
		calibration = Calibration((isYuv) ? CALIBRATION_STEP::STEP_1_RAW_YUV : CALIBRATION_STEP::STEP_2_CALIBRATE, isYuv);

		Info(_log, "The source is %s", (isYuv) ? "YUV" : "RGB");

		if (isYuv && !set1to1LUT())
		{			
			error("Could not allocated memory (~50MB) for internal temporary buffer. Stopped.");
			return;
		}				

		Debug(_log, "Requested LUT calibration. User settings: saturation = %0.2f, luminance = %0.2f, gammas = (%0.2f, %0.2f, %0.2f)",
			_saturation, _luminance, _gammaR, _gammaG, _gammaB);

		requestNextTestBoard(calibration.step);
	}
	else if ((checksum == CALIBRATION_STEP::STEP_1_RAW_YUV || checksum == CALIBRATION_STEP::STEP_2_CALIBRATE) &&
			!calibration.isVideoCapturingEnabled)
	{
		calibration.isVideoCapturingEnabled = true;
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
		error("Unknown request. Stopped.");
		return;
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

vec<double, 3> LutCalibrator::getColor(const Image<ColorRgb>& image, double blackLevelError, int logicX, int y, double scaleX, double scaleY)
{
	double3 color{ 0,0,0 };

	int sX = std::round((logicX * LOGIC_BLOCKS_X_SIZE + (y % 2) * 2 + 1 + 0.5) * scaleX);
	int sY = std::round((y + 0.5) * scaleY);

	double cR = 0, cG = 0, cB = 0;

	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++)
		{
			ColorRgb cur = image(sX + i, sY + j);
			color.x += cur.red;
			color.y += cur.green;
			color.z += cur.blue;
		}

	color /= 9;

	for (int i = -1; i <= 1; i += 2)
	{
		ColorRgb cur = image(sX + i * scaleX, sY);
		double3 control(cur.red, cur.green, cur.blue);

		if (getVecMax(control) > blackLevelError)
			throw std::invalid_argument("Invalid element/color detected in test image. Reliable analysis cannot be performed. Make sure the test board takes up the entire screen in the video live preview.");
	}

	return color;
}

uint16_t LutCalibrator::getCrc(const Image<ColorRgb>& image, double blackLevelError, double whiteLevelError, int y, double scaleX, double scaleY)
{
	uint16_t retVal = 0;
	for (int i = 0; i < 8; i++)
	{
		double3 color = getColor(image, blackLevelError, 2 + i, y, scaleX, scaleY);
		
		if (getVecMin(color) > whiteLevelError)
			retVal |= 1 << (7 - i);
		else if (getVecMax(color) > blackLevelError)
			throw std::invalid_argument("Unexpected brightness value in encoded CRC. Make sure the test board takes up the entire screen in the video live preview.");
	}
	return retVal;
}

class Colorspace
{
	public:

} colorspace;

void LutCalibrator::handleImage(const Image<ColorRgb>& image)
{
	//////////////////////////////////////////////////////////////////////////
	/////////////////////////  Verify source  ////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	if (image.width() < 3 * SCREEN_BLOCKS_X || image.height() < 3 * SCREEN_BLOCKS_Y)
	{
		error(QString("Too low resolution: 384x216 is the minimum. Received video frame: %1x%2. Stopped.").arg(image.width()).arg(image.height()));
		return;
	}

	if (image.width() * 1080 != image.height() * 1920)
	{
		error("Invalid resolution width/height ratio. Expected aspect: 1920/1080 (or the same 1280/720 etc). Stopped.");
		return;
	}

	try
	{
		//////////////////////////////////////////////////////////////////////////
		//////////////////////////  Verify frame  ////////////////////////////////
		//////////////////////////////////////////////////////////////////////////

		double blackLevelError = 48, whiteLevelError = 96;
		double scaleX = image.width() / static_cast<double>(SCREEN_BLOCKS_X);
		double scaleY = image.height() / static_cast<double>(SCREEN_BLOCKS_Y);

		double3 black = getColor(image, blackLevelError, 0, 0, scaleX, scaleY);
		blackLevelError = getVecMax(black) + 8;
		double3 white = getColor(image, blackLevelError, 1, 0, scaleX, scaleY);
		whiteLevelError = getVecMin(white) - 8;

		if (whiteLevelError <= blackLevelError)
			throw std::invalid_argument("The white color is lower than the black color in the captured image. Make sure the test board takes up the entire screen in the video live preview.");

		int topCrc = getCrc(image, blackLevelError, whiteLevelError, 0, scaleX, scaleY);
		int bottomCrc = getCrc(image, blackLevelError, whiteLevelError, SCREEN_BLOCKS_Y - 1, scaleX, scaleY);

		if (topCrc != bottomCrc)
			throw std::invalid_argument("The encoded CRC of the top line is different than the bottom line. Make sure the test board takes up the entire screen in the video live preview.");

		Debug(_log, "Current frame: crc = %i, black level = (%0.2f, %0.2f, %0.2f), white level = (%0.2f, %0.2f, %0.2f), ",
					topCrc, black.x, black.y, black.z, white.x, white.y, white.z);

		//////////////////////////////////////////////////////////////////////////
		////////////////////////  Colors Capturing  //////////////////////////////
		//////////////////////////////////////////////////////////////////////////

		calibration.colorRange = (getVecMax(black) > 2 || calibration.isYUV) ? YuvConverter::COLOR_RANGE::LIMITED : YuvConverter::COLOR_RANGE::FULL;
		Debug(_log, "Color range: %s", (calibration.colorRange == YuvConverter::COLOR_RANGE::LIMITED) ? "LIMITED" : "FULL");

		int actual = 0;
		for (int py = 1; py < SCREEN_BLOCKS_Y - 1; py++)
			for (int px = 0; px < LOGIC_BLOCKS_X; px++)
			{
				TEST_COLOR_ID prime = TEST_COLOR_ID::WHITE;
				int shadow = 0;
				Result result;

				if (!getSourceColor(actual++, result.source, prime, shadow))
				{
					py = SCREEN_BLOCKS_Y;
					break;
				}

				result.input = getColor(image, blackLevelError, px, py, scaleX, scaleY);

				if (calibration.isYUV)
				{
					result.inputYUV = result.input;
				}
				else
				{
					
					result.inputYUV = yuvConverter.toYuvBT709(calibration.colorRange, result.input/255.0) * 255.0;
					result.inputRGB = result.input;
				}

				calibration.results[prime][shadow] = result;
			}

		stopHandler();
		calibrate();

	}
	catch (const std::exception& ex)
	{
		Error(_log, ex.what());
	}
	catch (...)
	{
		Error(_log, "General exception");
	}
}


struct MappingPrime {
	TEST_COLOR_ID prime;
	double3 org;
	double3 real;
	double3 delta{};
};

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
}

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

std::list<MappingPrime> LutCalibrator::toneMapping()
{
	std::list<MappingPrime> m = {
		{ TEST_COLOR_ID::GREEN, {}, {} },
		{ TEST_COLOR_ID::GREEN_BLUE, {}, {} },
		{ TEST_COLOR_ID::BLUE, {}, {} },
		{ TEST_COLOR_ID::RED_BLUE, {}, {} },
		{ TEST_COLOR_ID::RED, {}, {} },
		{ TEST_COLOR_ID::RED_GREEN, {}, {} },
		{ TEST_COLOR_ID::RED_GREEN2, {}, {} },
		{ TEST_COLOR_ID::GREEN_BLUE2, {}, {} },
		{ TEST_COLOR_ID::RED_BLUE2, {}, {} },
		{ TEST_COLOR_ID::RED2_GREEN, {}, {} },
		{ TEST_COLOR_ID::GREEN2_BLUE, {}, {} },
		{ TEST_COLOR_ID::RED2_BLUE, {}, {} }
	};

	for (auto& c : m)
	{
		auto a = to_double3(TEST_COLORS.at(c.prime).first) / 255.0;
		c.org = xyz_to_lch(from_sRGB_to_XYZ(a) * 100.0);		

		int average = 0;
		for (int index = COLOR_DIVIDES - 1; index >= 0; index--)
		{
			auto b = calibration.results[c.prime][0].outputNormRGB;
			c.real = xyz_to_lch(from_sRGB_to_XYZ(b) * 100.0);
			c.delta += c.org - c.real;
			average++;
		}

		c.delta /= average;
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

	QStringList info, intro;
	info.append("Primaries in LCH colorspace");
	info.append("name,      RGB primary in LCH,     captured primary in LCH       |  primary RGB, captured RGB  |   average LCH delta       |  LCH to RGB way back ");
	info.append("--------------------------------------------------------------------------------------------------------------------------------------------------------");
	for (auto& c : m)
	{
		auto aa = from_XYZ_to_sRGB(lch_to_xyz(c.org) / 100.0) * 255;
		auto bb = from_XYZ_to_sRGB(lch_to_xyz(c.real) / 100.0) * 255;
		info.append(QString("%1 %2 %3 | %4 %5 | %6 | %7 %8").arg(TEST_COLORS.at(c.prime).second + ":", 12).
											arg(vecToString(c.org)).
											arg(vecToString(c.real)).
											arg(vecToString(TEST_COLORS.at(c.prime).first)).
											arg(vecToString(to_byte3(calibration.results[c.prime][0].outputRGB))).
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

	for (auto& c : calibration.results)
	{
		bool firstLine = true;
		for (auto& p : calibration.results[c.first])
		{
			auto r = p.outputNormRGB;

			doToneMapping(m, r);

			p.outputRGB = r * 255.0;

			if (std::exchange(firstLine, false))
				info.append(QString("%1: %2 => %3 => %4 => %5").
					arg(TEST_COLORS.at(c.first).second + ":", 12).
					arg(vecToString(TEST_COLORS.at(c.first).first)).
					arg(vecToString(to_byte3(p.inputRGB * 255))).
					arg(vecToString(to_byte3(p.outputNormRGB * 255))).
					arg(vecToString(to_byte3(p.outputRGB)))
				);
		}
	}
	info.append("-------------------------------------------------------------------------------------------------");
	sendReport(info.join("\r\n"));

	return m;
}

void LutCalibrator::tryHDR10()
{
	// data always incomes as yuv
	for (auto& ref : calibration.results)
		for (auto& result : ref.second)
		{
			result.inputRGB = yuvConverter.toRgb(YuvConverter::COLOR_RANGE::LIMITED, YuvConverter::YUV_COEFS::BT709, result.inputYUV / 255.0);
		}

	// detect nits
	double3 nits = calibration.results[TEST_COLOR_ID::WHITE][0].inputRGB;

	calibration.nits = 10000.0 * eotf(1.0, maxelem(nits));
	Debug(_log, "Assuming the signal is HDR, it is calibrated for %0.2f nits", calibration.nits);


	/*double3 red = calibration.results[TEST_COLOR_ID::RED][0].inputRGB;
	Debug(_log, "%s", QSTRING_CSTR(vecToString(XYZ_to_xy(from_bt2020_to_XYZ(red)))));
	auto re = double3(eotf(10000.0 / calibration.nits, red.x),
		eotf(10000.0 / calibration.nits, red.y),
		eotf(10000.0 / calibration.nits, red.z));
	Debug(_log, "%s", QSTRING_CSTR(vecToString(XYZ_to_xy(from_bt2020_to_XYZ(re)))));*/
	// apply PQ and gamma
	for (auto& ref : calibration.results)
		for (auto& result : ref.second)
		{
			const auto& a = result.inputRGB;
			auto e = double3(eotf(10000.0 / calibration.nits, a.x),
				eotf(10000.0 / calibration.nits, a.y),
				eotf(10000.0 / calibration.nits, a.z));

			result.processingXYZ = from_bt2020_to_XYZ(e);

			//sRgbCutOff(result.processingXYZ);

			auto srgb = from_XYZ_to_sRGB(result.processingXYZ);
			result.outputRGB = double3(clamp(ootf(srgb.x), 0.0, 1.0),
										clamp(ootf(srgb.y), 0.0, 1.0),
										clamp(ootf(srgb.z), 0.0, 1.0));
			result.outputNormRGB = result.outputRGB;
			result.outputRGB *= 255.0;
		}

	// correct gamut shift
	auto m = toneMapping();

	if (!true)
	{
		// build LUT table
		_lut.resize(LUT_FILE_SIZE);
		for (int g = 0; g <= 255; g++)
			for (int b = 0; b <= 255; b++)
				for (int r = 0; r <= 255; r++)
				{
					auto a = double3(r / 255.0, g / 255.0, b / 255.0);

					auto e = double3(eotf(10000.0 / calibration.nits, a.x),
						eotf(10000.0 / calibration.nits, a.y),
						eotf(10000.0 / calibration.nits, a.z));

					auto srgb = from_XYZ_to_sRGB(from_bt2020_to_XYZ(e));
					auto ready = double3(clamp(ootf(srgb.x), 0.0, 1.0),
						clamp(ootf(srgb.y), 0.0, 1.0),
						clamp(ootf(srgb.z), 0.0, 1.0));

					doToneMapping(m, ready);

					//ready = acesToneMapping(ready);
					//ready = uncharted2_filmic(ready);					

					byte3 result = to_byte3(ready * 255.0);

					// save it
					uint32_t ind_lutd = LUT_INDEX(r, g, b);
					_lut.data()[ind_lutd] = result.x;
					_lut.data()[ind_lutd + 1] = result.y;
					_lut.data()[ind_lutd + 2] = result.z;
				}
		/*
		QImage f;
		f.load("D:/test_image.png");
		for(int i = 0; i < f.width(); i++)
			for (int j = 0; j < f.height(); j++)
			{
				QColor c = f.pixelColor(QPoint(i, j));
				uint32_t ind_lutd = LUT_INDEX(c.red(), c.green(), c.blue());
				QColor d(_lut.data()[ind_lutd], _lut.data()[ind_lutd + 1], _lut.data()[ind_lutd + 2]);
				f.setPixelColor(QPoint(i, j), d);
			}
		f.save("D:/test_image_output.png");
		*/
		
	}

	printFullReport();
	
}

void LutCalibrator::printFullReport()
{
	
	QStringList ret, report;


	for (auto& c : calibration.results)
	{
		ret.append(TEST_COLORS.at(c.first).second);
		for (auto& p : calibration.results[c.first])
		{
			ret.append(ColorSpaceMath::vecToString(to_byte3(p.outputRGB)));
		}
	}

	int step = COLOR_DIVIDES + 1;
	for (int i = 0; i + 4 * step < ret.size(); i+= 3 * step)
		for (int j = 0; j < step; j++)
			report.append(QString("%1 %2 %3 %4").arg(ret[i], -16).arg(ret[i + step], -16).arg(ret[i + step * 2], -16).arg(ret[(i++) + step * 3], -32));

   sendReport(report.join("\r\n"));	
}

void LutCalibrator::setupWhitePointCorrection()
{	
	

	for (const auto& coeff : yuvConverter.knownCoeffs)
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
		sendReport(yuvConverter.toString());
	#endif

	sendReport("Captured colors:\r\n" +
				generateShortReport([](const Result& res) {
					return vecToString(
						to_byte3(
							
							res.inputRGB));
				}));

	tryHDR10();

	sendReport("HDR10:\r\n" +
		generateShortReport([](const Result& res) {
				return vecToString(to_byte3(res.outputRGB));
			}));
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

	// white point correction
	double nits = 200;
	linalg::mat<double, 3, 3> convert_bt2020_to_XYZ;
	linalg::mat<double, 3, 3> convert_XYZ_to_sRgb;
	whitePointCorrection(nits, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb);

	// build LUT table
	for (int g = 0; g <= 255; g++)
		for (int b = 0; b <= 255; b++)
			for (int r = 0; r <= 255; r++)
			{
				double Ri, Gi, Bi;

				if (strategy == 3)
				{
					linalg::vec<double, 3> inputPoint(r, g, b);
					inputPoint.x = eotf(10000.0 / nits, inputPoint.x / 255.0);
					inputPoint.y = eotf(10000.0 / nits, inputPoint.y / 255.0);
					inputPoint.z = eotf(10000.0 / nits, inputPoint.z / 255.0);

					inputPoint = linalg::mul(convert_bt2020_to_XYZ, inputPoint);
					inputPoint = linalg::mul(convert_XYZ_to_sRgb, inputPoint);

					Ri = inputPoint.x;
					Gi = inputPoint.y;
					Bi = inputPoint.z;
				}
				else
				{
					Ri = clampDouble((r * whiteBalance.scaledRed - floor) / scale, 0, 1.0);
					Gi = clampDouble((g * whiteBalance.scaledGreen - floor) / scale, 0, 1.0);
					Bi = clampDouble((b * whiteBalance.scaledBlue - floor) / scale, 0, 1.0);
				}

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
					//fromBT2020toBT709(Ri, Gi, Bi, Ri, Gi, Bi);					
					linalg::vec<double, 3> inputPoint(Ri, Gi, Bi);

					inputPoint = mul(convert_bt2020_to_XYZ, inputPoint);
					inputPoint = mul(convert_XYZ_to_sRgb, inputPoint);

					Ri = inputPoint.x;
					Gi = inputPoint.y;
					Bi = inputPoint.z;
				}

				// ootf
				if (strategy == 0 || strategy == 1 || strategy == 3)
				{
					Ri = ootf(Ri);
					Gi = ootf(Gi);
					Bi = ootf(Bi);
				}

				double finalR = clampDouble(Ri, 0, 1.0);
				double finalG = clampDouble(Gi, 0, 1.0);
				double finalB = clampDouble(Bi, 0, 1.0);

				//balanceGray(r, g, b, finalR, finalG, finalB);

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

void LutCalibrator::whitePointCorrection(double& nits, linalg::mat<double, 3, 3>& convert_bt2020_to_XYZ, linalg::mat<double, 3, 3>& convert_XYZ_to_corrected)
{
	double max = std::min(_maxColor.red, std::min( _maxColor.green , _maxColor.blue));
	nits = 10000.0 * eotf(1.0, max / 255.0);

	std::vector<vec<double, 3>> actualPrimaries{
		tovec(_colorBalance[capColors::Red]), tovec(_colorBalance[capColors::Green]), tovec(_colorBalance[capColors::Blue]), tovec(_colorBalance[capColors::White])
	};

	for (auto& c : actualPrimaries)
	{
		c.x = eotf(10000.0 / nits, c.x / 255.0);
		c.y = eotf(10000.0 / nits, c.y / 255.0);
		c.z = eotf(10000.0 / nits, c.z / 255.0);
	}

	linalg::vec<double, 2> bt2020_red_xy(0.708, 0.292);
	linalg::vec<double, 2> bt2020_green_xy(0.17, 0.797);
	linalg::vec<double, 2> bt2020_blue_xy(0.131, 0.046);
	linalg::vec<double, 2> bt2020_white_xy(0.3127, 0.3290);


	convert_bt2020_to_XYZ = to_XYZ(bt2020_red_xy, bt2020_green_xy, bt2020_blue_xy, bt2020_white_xy);

	vec<double, 2> sRgb_red_xy = { 0.64f, 0.33f };
	vec<double, 2> sRgb_green_xy = { 0.30f, 0.60f };
	vec<double, 2> sRgb_blue_xy = { 0.15f, 0.06f };
	vec<double, 2> sRgb_white_xy = { 0.3127f, 0.3290f };

	vec<double, 3> actual_red_xy(actualPrimaries[0]);
	actual_red_xy = linalg::mul(convert_bt2020_to_XYZ, actual_red_xy);
	sRgb_red_xy = XYZ_to_xy(actual_red_xy);

	vec<double, 3> actual_green_xy(actualPrimaries[1]);
	actual_green_xy = mul(convert_bt2020_to_XYZ, actual_green_xy);
	sRgb_green_xy = XYZ_to_xy(actual_green_xy);

	vec<double, 3> actual_blue_xy(actualPrimaries[2]);
	actual_blue_xy = mul(convert_bt2020_to_XYZ, actual_blue_xy);
	sRgb_blue_xy = XYZ_to_xy(actual_blue_xy);

	vec<double, 3> actual_white_xy(actualPrimaries[3]);
	actual_white_xy = mul(convert_bt2020_to_XYZ, actual_white_xy);
	sRgb_white_xy = XYZ_to_xy(actual_white_xy);

	mat<double, 3, 3> convert_sRgb_to_XYZ;
	convert_sRgb_to_XYZ = to_XYZ(sRgb_red_xy, sRgb_green_xy, sRgb_blue_xy, sRgb_white_xy);

	convert_XYZ_to_corrected = inverse(convert_sRgb_to_XYZ);

	Debug(_log, "YUV coefs: %s", REC(_currentCoef));
	Debug(_log, "Nits: %f", nits);
	Debug(_log, "r: (%.3f, %.3f) vs (%.3f, %.3f)", sRgb_red_xy.x, sRgb_red_xy.y, 0.64f, 0.33f);
	Debug(_log, "g: (%.3f, %.3f) vs (%.3f, %.3f)", sRgb_green_xy.x, sRgb_green_xy.y, 0.30f, 0.60f);
	Debug(_log, "b: (%.3f, %.3f) vs (%.3f, %.3f)", sRgb_blue_xy.x, sRgb_blue_xy.y, 0.15f, 0.06f);
	Debug(_log, "w: (%.3f, %.3f) vs (%.3f, %.3f)", sRgb_white_xy.x, sRgb_white_xy.y, 0.3127f, 0.3290f);
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

	// white point correction
	double nits = 200;
	mat<double, 3, 3> convert_bt2020_to_XYZ;
	mat<double, 3, 3> convert_XYZ_to_sRgb;
	whitePointCorrection(nits, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb);

	for (int whiteIndex = capColors::Gray1; whiteIndex <= capColors::White; whiteIndex++)
	{
		ColorStat whiteBalance = _colorBalance[whiteIndex];

		for (int scale = (qRound(ceiling) / 8) * 8, limitScale = 512; scale <= limitScale; scale = (scale == limitScale) ? limitScale + 1 : qMin(scale + 4, limitScale))
			for (int strategy = 0; strategy < 4; strategy++)
				for (double range = rangeStart; range <= rangeLimit; range += (range < 5) ? 0.1 : 0.5)
					if ((strategy != 2 && strategy != 3) || range == rangeStart)
					{
						double currentError = 0;
						QList<QString> colors;
						double lR = -1, lG = -1, lB = -1;

						for (int ind : primaries)
						{
							ColorStat calculated, normalized = _colorBalance[ind];

							if (strategy == 3)
							{
								vec<double, 3> inputPoint(_colorBalance[ind].red, _colorBalance[ind].green, _colorBalance[ind].blue);
								inputPoint.x = eotf(10000.0 / nits, inputPoint.x / 255.0);
								inputPoint.y = eotf(10000.0 / nits, inputPoint.y / 255.0);
								inputPoint.z = eotf(10000.0 / nits, inputPoint.z / 255.0);

								inputPoint = mul(convert_bt2020_to_XYZ, inputPoint);
								inputPoint = mul(convert_XYZ_to_sRgb, inputPoint);

								calculated = ColorStat(inputPoint.x, inputPoint.y, inputPoint.z);
							}
							else
							{
								normalized /= (double)scale;

								normalized.red *= whiteBalance.scaledRed;
								normalized.green *= whiteBalance.scaledGreen;
								normalized.blue *= whiteBalance.scaledBlue;
							}

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
								//fromBT2020toBT709(normalized.red, normalized.green, normalized.blue, calculated.red, calculated.green, calculated.blue);

								vec<double, 3> inputPoint(normalized.red, normalized.green, normalized.blue);

								inputPoint = mul(convert_bt2020_to_XYZ, inputPoint);
								inputPoint = mul(convert_XYZ_to_sRgb, inputPoint);

								calculated = ColorStat(inputPoint.x, inputPoint.y, inputPoint.z);
							}

							// ootf
							if (strategy == 0 || strategy == 1 || strategy == 3)
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
