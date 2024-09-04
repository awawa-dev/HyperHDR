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
	return true;
};

LutCalibrator::LutCalibrator()
{
	_log = Logger::getInstance("CALIBRATOR");
	_capturedColors = std::make_shared<CapturedColors>();
	_yuvConverter = std::make_shared<YuvConverter>();
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
			auto yuv = testColor.yuv();

			auto rgbBT709 =_yuvConverter->toRgb(_capturedColors->getRange(), YuvConverter::BT709, yuv) * 255.0;

			if (!full)
				rep.append(QString("%1: %2 => %3 , YUV: %4")
					.arg(QString::fromStdString(color.first), 12)
					.arg(vecToString(testColor.getSourceRGB()), 12)
					.arg(vecToString(ColorSpaceMath::to_byte3(rgbBT709)), 12)
					.arg(vecToString(ColorSpaceMath::to_byte3(yuv * 255.0)), 12));
			else
				rep.append(QString("%1: %2 => %3 [corrected]")
					.arg(QString::fromStdString(color.first), 12)
					.arg(vecToString(testColor.getSourceRGB()), 12)
					.arg(vecToString(testColor.getFinalRGB()), 12));

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

void LutCalibrator::incomingCommand(QString rootpath, GrabberWrapper* grabberWrapper, hyperhdr::Components defaultComp, int checksum, double saturation, double luminance, double gammaR, double gammaG, double gammaB)
{
	_rootPath = rootpath;

	stopHandler();

	_capturedColors.reset();
	_capturedColors = std::make_shared<CapturedColors>();

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

	if (!parseBoard(_log, image, boardIndex, (*_capturedColors.get())))
	{		
		return;
	}

	
	_capturedColors->setCaptured(boardIndex);


	if (_capturedColors->areAllCaptured())
	{
		Info(_log, "All boards are captured. Starting calibration...");
		stopHandler();
		calibrate();
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

void LutCalibrator::toneMapping()
{
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

	QStringList info, intro;
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

	for (int r = 0; r < SCREEN_COLOR_DIMENSION; r+=4)
		for (int g = 0; g < SCREEN_COLOR_DIMENSION; g+=4)
			for (int b = 0; b < SCREEN_COLOR_DIMENSION; b+=4)
			{
				bool firstLine = true;
				auto sample = _capturedColors->all[r][g][b];



				auto correct = to_double3(sample.getFinalRGB()) / 255.0;

				doToneMapping(m, correct);

				auto corrected = to_byte3(correct * 255.0);

				//if (std::exchange(firstLine, false))
					info.append(QString("%1 => %2 => %3").
						arg(vecToString(sample.getSourceRGB())).
						arg(vecToString(sample.getFinalRGB())).
						arg(vecToString(corrected))
					);								
			}	
	info.append("-------------------------------------------------------------------------------------------------");
	sendReport(info.join("\r\n"));
}


void LutCalibrator::tryHDR10()
{
	// detect nits
	const int SCALE = SCREEN_COLOR_DIMENSION - 1;
	const auto white = _capturedColors->all[SCALE][SCALE][SCALE].Y();
	double nits = 0;

	if (_capturedColors->getRange() == YuvConverter::COLOR_RANGE::LIMITED)
	{
		nits = 10000.0 * PQ_ST2084(1.0, (white - 16.0) / (235.0 - 16.0));
	}
	else
	{
		nits = 10000.0 * PQ_ST2084(1.0, white / 255.0);
	}

	Debug(_log, "Assuming the signal is HDR, it is calibrated for %0.2f nits", nits);	

	double minError = std::numeric_limits<double>::max() / 2, currentError = 0;
	double3x3 convert_bt2020_to_XYZ;
	double3x3 convert_XYZ_to_sRgb;

	struct {
		YuvConverter::YUV_COEFS coef = YuvConverter::YUV_COEFS::FCC;
		double2 coefDelta;
		double3 aspect;
		bool bt2020Range = false;
		bool altConvert = false;;
	} bestResult;

	auto scoreBoard = [this](bool testOnly, int coef, double2 coefDelta, int nits, double3 aspect, bool tryBt2020Range, bool altConvert, const double3x3& bt2020_to_sRgb, const double& minError, double& currentError) {
		auto coefValues = _yuvConverter->getCoef(YuvConverter::YUV_COEFS(coef)) + coefDelta;
		auto coefMatrix = _yuvConverter->create_yuv_to_rgb_matrix(_capturedColors->getRange(), coefValues.x, coefValues.y);
		
		for (int r = 0; r < SCREEN_COLOR_DIMENSION; r++)
			for (int g = 0; g < SCREEN_COLOR_DIMENSION; g++)
				for (int b = 0; b < SCREEN_COLOR_DIMENSION; b++)
					if (!testOnly || ((r % 4 == 0 && g % 4 == 0 && b % 4 == 0) && (r != g || g != b || r == SCREEN_COLOR_DIMENSION - 2)))
					{
						auto& sample = _capturedColors->all[r][g][b];
						auto yuv = sample.yuv();

						yuv.x = (yuv.x - 0.5) * aspect.x + 0.5;
						if (sample.U() != sample.V() || sample.U() < 127 || sample.U() > 129)
						{							
							yuv.y = (yuv.y - 0.5) * aspect.y + 0.5;
							yuv.z = (yuv.z - 0.5) * aspect.z + 0.5;
						}

						auto a = _yuvConverter->multiplyColorMatrix(coefMatrix,yuv);

						auto e = PQ_ST2084(10000.0 / nits, a);

						double3 processingXYZ, srgb;
						if (altConvert)
						{							
							srgb = mul(bt2020_to_sRgb, e);
						}
						else
						{
							srgb = ColorSpaceMath::from_BT2020_to_BT709(e);
						}

						srgb = srgb_linear_to_nonlinear(srgb);

						if (tryBt2020Range)
						{
							srgb = bt2020_nonlinear_to_linear(srgb);							
							srgb = srgb_linear_to_nonlinear(srgb);
						}						

						if (testOnly)
						{
							srgb *= 255;
							//currentError += sample.getSourceError(srgb);
							currentError += getError(sample.getSourceRGB(), ColorSpaceMath::to_byte3(srgb));

							if (r + 2 == SCREEN_COLOR_DIMENSION && g + 2 == SCREEN_COLOR_DIMENSION && b + 2 == SCREEN_COLOR_DIMENSION && linalg::maxelem(srgb) > 250)
								currentError = minError;

							if (currentError >= minError)
								return;
						}
						else
						{
							sample.setFinalRGB(srgb);
						}
					}
		};	


	int coefStarter = YuvConverter::YUV_COEFS::FCC;
	int coefEnd = YuvConverter::YUV_COEFS::BT2020;
	double aspectXStarter = 0.95;
	double aspectYStarter = 0.95;
	double aspectZStarter = 0.95;
	double aspectXEnd = 1.1;
	double aspectYEnd = 1.1;
	double aspectZEnd = 1.1;
	double aspectDeltaX = 0.025,aspectDeltaY = 0.025,aspectDeltaZ = 0.025;
	double krDeltaStart = -0.015, krDeltaEnd = 0.015;
	double kbDeltaStart = -0.015, kbDeltaEnd = 0.015;
	int tryBt2020RangeStarter = 0, tryBt2020RangeEnd = 1;
	int altConvertStarter = 0, altConvertEnd = 1;

	for (int i = 0; i < 2; i++)
	{
		
		for (int coef = coefStarter; coef <= coefEnd; coef++)
		{
			capturedPrimariesCorrection(nits, coef, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb);
			auto bt2020_to_sRgb = mul(convert_XYZ_to_sRgb, convert_bt2020_to_XYZ);

			for (double krDelta = krDeltaStart; krDelta <= krDeltaEnd; krDelta += 0.001)
				for (double kbDelta = kbDeltaStart; kbDelta <= kbDeltaEnd; kbDelta += 0.001)
					for (double aspectX = aspectXStarter; aspectX <= aspectXEnd; aspectX += aspectDeltaX)
						for (double aspectY = aspectYStarter; aspectY <= aspectYEnd; aspectY += aspectDeltaY)
							for (double aspectZ = aspectZStarter; aspectZ <= aspectZEnd; aspectZ += aspectDeltaZ)
								for (int tryBt2020Range = tryBt2020RangeStarter; tryBt2020Range <= tryBt2020RangeEnd; tryBt2020Range++)
									for (int altConvert = altConvertStarter; altConvert <= altConvertEnd; altConvert++)
									{
										currentError = 0;

										scoreBoard(true, coef, double2(krDelta, kbDelta), nits, double3(aspectX, aspectY, aspectZ), tryBt2020Range, altConvert, bt2020_to_sRgb, minError, currentError);

										if (currentError < minError)
										{
											minError = currentError;
											bestResult.coef = YuvConverter::YUV_COEFS(coef);
											bestResult.coefDelta = double2(krDelta, kbDelta);
											bestResult.bt2020Range = tryBt2020Range;
											bestResult.altConvert = altConvert;
											bestResult.aspect = double3(aspectX, aspectY, aspectZ);
										}
									}

		}

		coefStarter = coefEnd = bestResult.coef;
		aspectXStarter = std::max(bestResult.aspect.x - 0.05, 0.95);
		aspectYStarter = std::max(bestResult.aspect.y - 0.05, 0.95);
		aspectZStarter = std::max(bestResult.aspect.z - 0.05, 0.95);
		aspectXEnd = std::min(bestResult.aspect.x + 0.05, 1.1);
		aspectYEnd = std::min(bestResult.aspect.y + 0.05, 1.1);
		aspectZEnd = std::min(bestResult.aspect.z + 0.05, 1.1);

		aspectDeltaX = (aspectXEnd - aspectXStarter) / 50;
		aspectDeltaY = (aspectYEnd - aspectYStarter) / 50;
		aspectDeltaZ = (aspectZEnd - aspectZStarter) / 50;

		krDeltaStart = krDeltaEnd = bestResult.coefDelta.x;
		kbDeltaStart = kbDeltaEnd = bestResult.coefDelta.y;

		tryBt2020RangeStarter = tryBt2020RangeEnd = bestResult.bt2020Range;

		altConvertStarter = altConvertEnd = bestResult.altConvert;		
	}

	capturedPrimariesCorrection(nits, bestResult.coef, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb);
	auto bt2020_to_sRgb = mul(convert_XYZ_to_sRgb, convert_bt2020_to_XYZ);
	scoreBoard(false, bestResult.coef, bestResult.coefDelta, nits, bestResult.aspect, bestResult.bt2020Range, bestResult.altConvert, bt2020_to_sRgb, minError, currentError);

	Debug(_log, "Score: %f", minError / 10.0);
	Debug(_log, "Selected coef: %s", QSTRING_CSTR( _yuvConverter->coefToString(bestResult.coef)));
	Debug(_log, "selected coef delta: %f %f", bestResult.coefDelta.x, bestResult.coefDelta.y);
	Debug(_log, "Selected nits: %f", nits);
	Debug(_log, "selected bt2020 range: %i", bestResult.bt2020Range);
	Debug(_log, "selected alt convert: %i", bestResult.altConvert);
	Debug(_log, "selected aspect: %f %f %f", bestResult.aspect.x, bestResult.aspect.y, bestResult.aspect.z);

	/*
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
	YuvConverter converter;
	#ifndef NDEBUG
		sendReport(converter.toString());
	#endif

	sendReport("Captured colors:\r\n" +
				generateReport(false));

	tryHDR10();

	sendReport("HDR10:\r\n" +
		generateReport(true));

	toneMapping();
}









double LutCalibrator::getError(const byte3& first, const byte3& second)
{
	
	double errorR = 0, errorG = 0, errorB = 0;

	if ((first[0] == 255 || first[0] == 128) && first[1] == 0 && first[2] == 0)
	{
		if (second[0] <= 1)
			return std::pow(255, 2) * 3;

		errorG = 100.0 * second[1] / second[0];
		errorB = 100.0 * second[2] / second[0];

		if (first[0] == 255)
			errorR = (255.0 - second[0]);
		else if (first[0] == 128)
			errorR = (128.0 - second[0]);
	}
	else if (first[0] == 0 && (first[1] == 255 || first[1] == 128)  && first[2] == 0)
	{
		if (second[1] <= 1)
			return std::pow(255, 2) * 3;

		errorR = 100.0 * second[0] / second[1];
		errorB = 100.0 * second[2] / second[1];

		if (first[1] == 255)
			errorG = (255.0 - second[1]);
		else if (first[1] == 128)
			errorG = (128.0 - second[1]);
	}
	else if (first[0] == 0 && first[1] == 0 && (first[2] == 255 || first[2] == 128))
	{
		if (second[2] <= 1)
			return std::pow(255, 2) * 3;

		errorR = 100.0 * second[0] / second[2];
		errorG = 100.0 * second[1] / second[2];

		if (first[2] == 255)
			errorB = (255.0 - second[2]);
		else if (first[2] == 128)
			errorB = (128.0 - second[2]);
	}

	else if (first[0] == first[1] && (first[1] == 255 || first[1] == 128) && first[2] == 0)
	{
		if (second[0] <= 1)
			return std::pow(255, 2) * 3;

		errorR = ((double)second[0] - second[1]);
		errorG = second[2];
		if (first[1] == 255)
			errorB = std::abs(255.0 - second[1]) + std::abs(255.0 - second[0]);
		else if (first[1] == 128)
			errorB = std::abs(128.0 - second[1]) + std::abs(128.0 - second[0]);
	}
	else if (first[0] == first[2] && (first[2] == 255 || first[2] == 128) && first[1] == 0)
	{
		if (second[0] <= 1)
			return std::pow(255, 2) * 3;

		errorR = ((double)second[0] - second[2]);
		errorG = second[1];
		if (first[2] == 255)
			errorB = std::abs(255.0 - second[2]) + std::abs(255.0 - second[0]);
		else if (first[2] == 128)
			errorB = std::abs(128.0 - second[2]) + std::abs(128.0 - second[0]);
	}
	else if (first[1] == first[2] && (first[1] == 255 || first[1] == 128) && first[0] == 0)
	{
		if (second[2] <= 1)
			return std::pow(255, 2) * 3;

		errorR = ((double)second[2] - second[1]);
		errorG = second[0];
		if (first[1] == 255)
			errorB = std::abs(255.0 - second[1]) + std::abs(255.0 - second[2]);
		else if (first[1] == 128)
			errorB = std::abs(128.0 - second[1]) + std::abs(128.0 - second[2]);
	}
	else if (first[0] == 255 && first[1] == 255 && first[2] == 64)
	{
		if (second[0] <= 200 || second[1] <= 200 || second[2] < 32)
			return std::pow(255, 2) * 3;

		errorR = (double)second[0] - second[1];
		errorG = (double)second[0] - second[1];
		errorB = (second[2] - 64.0) / 2;
	}
	else if (first[0] == 255 && first[1] == 64 && first[2] == 255)
	{
		if (second[0] <= 200  || second[1] < 32 || second[2] <= 200)
			return std::pow(255, 2) * 3;

		errorR = (double)second[0] - second[2];
		errorG = (second[1] - 64.0) / 2;
		errorB = (double)second[0] - second[2];
	}
	else if (first[0] == 64 && first[1] == 255 && first[2] == 255)
	{
		if (second[0] < 32 || second[1] <= 200 || second[2] <= 200 )
			return std::pow(255, 2) * 3;

		errorR = (second[0] - 64.0) / 2;
		errorG = (double)second[1] - second[2];
		errorB = (double)second[1] - second[2];
	}

	else if (first[0] == 255 && first[1] == 0 && first[2] == 128)
	{
		if (second[0] <= 1)
			return std::pow(255, 2) * 3;

		errorR = 2 * 100.0 * (1 - 2.0 * second[2] / second[0]);
		errorG = second[0] - 255.0;
		errorB = second[2] - 128.0;
	}
	else if (first[0] == 255 && first[1] == 128 && first[2] == 0)
	{
		if (second[0] <= 1)
			return std::pow(255, 2) * 3;

		errorR = 2 * 100.0 * (1 - 2.0 * second[1] / second[0]);
		errorG = second[0] - 255.0;
		errorB = second[1] - 128.0;
	}
	else if (first[0] == 0 && first[1] == 128 && first[2] == 255)
	{
		if (second[2] <= 1)
			return std::pow(255, 2) * 3;

		errorR = 2 * 100.0 * (1 - 2.0 * second[1] / second[2]);
		errorG = second[2] - 255.0;
		errorB = second[1] - 128.0;
	}
	else if (first[0] == 128 && first[1] == 64 && first[2] == 0)
	{
		if (second[0] <= 1)
			return std::pow(255, 2) * 3;

		errorR = second[0] - 128.0;
		errorG = 2 * 100.0 * (1 - 2.0 * second[1] / second[0]);
		errorB = second[1] - 64.0;
	}
	else if (first[0] == 128 && first[1] == 0 && first[2] == 64)
	{
		if (second[0] <= 1)
			return std::pow(255, 2) * 3;

		errorR = second[0] - 128.0;
		errorG = second[2] - 64.0;
		errorB = 2 * 100.0 * (1 - 2.0 * second[2] / second[0]);
	}
	else if (first[0] == 0 && first[1] == 64 && first[2] == 128)
	{
		if (second[2] <= 1)
			return std::pow(255, 2) * 3;

		errorR = second[2] - 128.0;
		errorG = 2 * 100.0 * (1 - 2.0 * second[1] / second[2]);
		errorB = second[1] - 64.0;
		}
	else if (first[0] == 0 && first[1] == 128 && first[2] == 64)
	{
		if (second[1] <= 1)
			return std::pow(255, 2) * 3;

		errorR = second[1] - 128.0;
		errorG = second[2] - 64.0;
		errorB = 2 * 100.0 * (1 - 2.0 * second[2] / second[1]);
	}
	else if (first[0] == 64 && first[1] == 0 && first[2] == 128)
	{
		if (second[2] <= 1)
			return std::pow(255, 2) * 3;

		errorR = second[2] - 128.0;
		errorG = 2 * 100.0 * (1 - 2.0 * second[0] / second[2]);
		errorB = second[0] - 64.0;
		}
	else if (first[0] == 64 && first[1] == 128 && first[2] == 0)
	{
		if (second[1] <= 1)
			return std::pow(255, 2) * 3;

		errorR = second[1] - 128.0;
		errorG = second[0] - 64.0;
		errorB = 2 * 100.0 * (1 - 2.0 * second[0] / second[1]);
		}
	else if (first[0] == 64 || first[1] == 64 || first[2] == 64)
	{

		if (first[0] == 64)
			errorR = (second[0] - 64.0) / 2;
		if (first[1] == 64)
			errorG = (second[1] - 64.0) / 2;
		if (first[2] == 64)
			errorB = (second[2] - 64.0) / 2;
	}
	else if ((first[0] == 255 || first[0] == 192 || first[0] == 128) &&
			 (first[1] == 255 || first[1] == 192 || first[1] == 128) &&
			 (first[2] == 255 || first[2] == 192 || first[2] == 128))
	{
		if (first[0] == 255)
			errorR = (255.0 - second[0]);
		else if (first[0] == 192)
			errorR = (192.0 - second[0]);
		else if (first[0] == 128)
			errorR = (128.0 - second[0]);


		if (first[1] == 255)
			errorG = (255.0 - second[1]);
		else if (first[1] == 192)
			errorG = (192.0 - second[1]);
		else if (first[1] == 128)
			errorG = (128.0 - second[1]);

		if (first[2] == 255)
			errorB = (255.0 - second[2]);
		else if (first[2] == 192)
			errorB = (192.0 - second[2]);
		else if (first[2] == 128)
			errorB = (128.0 - second[2]);

		errorR /= 4.0;
		errorG /= 4.0;
		errorB /= 4.0;

	}
	/*else if (first[1] > 0 && first[2] == first[1] && first[0] == first[1])
	{
		errorR = 200 * (first[0] - second[0]);
		errorB = 200 * (first[1] - second[1]);
		errorG = 200 * (first[2] - second[2]);
	}*/

	return std::pow(errorR, 2) + std::pow(errorG, 2) + std::pow(errorB, 2);

}

void LutCalibrator::capturedPrimariesCorrection(double nits, int coef, linalg::mat<double, 3, 3>& convert_bt2020_to_XYZ, linalg::mat<double, 3, 3>& convert_XYZ_to_corrected)
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
		a = PQ_ST2084(10000.0 / nits, a);
		actualPrimaries.push_back(a);
	}

	linalg::vec<double, 2> bt2020_red_xy(0.708, 0.292);
	linalg::vec<double, 2> bt2020_green_xy(0.17, 0.797);
	linalg::vec<double, 2> bt2020_blue_xy(0.131, 0.046);
	linalg::vec<double, 2> bt2020_white_xy(0.3127, 0.3290);


	convert_bt2020_to_XYZ = to_XYZ(bt2020_red_xy, bt2020_green_xy, bt2020_blue_xy, bt2020_white_xy);

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

	Debug(_log, "--------------------------------- Actual PQ primaries for YUV coefs: %s ---------------------------------", QSTRING_CSTR(_yuvConverter->coefToString(YuvConverter::YUV_COEFS(coef))));
	Debug(_log, "r: (%.3f, %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f)", sRgb_red_xy.x, sRgb_red_xy.y, 0.64f, 0.33f, 0.708, 0.292);
	Debug(_log, "g: (%.3f, %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f)", sRgb_green_xy.x, sRgb_green_xy.y, 0.30f, 0.60f, 0.17, 0.797);
	Debug(_log, "b: (%.3f, %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f)", sRgb_blue_xy.x, sRgb_blue_xy.y, 0.15f, 0.06f, 0.131, 0.046);
	Debug(_log, "w: (%.3f, %.3f) vs sRGB(%.3f, %.3f) vs bt2020(%.3f, %.3f)", sRgb_white_xy.x, sRgb_white_xy.y, 0.3127f, 0.3290f, 0.3127f, 0.3290f);
}

double LutCalibrator::fineTune(double& optimalRange, double& optimalScale, int& optimalWhite, int& optimalStrategy)
{
	throw std::exception();


	/*

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
	capturedPrimariesCorrection(nits, convert_bt2020_to_XYZ, convert_XYZ_to_sRgb);

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

	*/
}

bool LutCalibrator::finalize(bool fastTrack)
{
	/*QString fileName = QString("%1%2").arg(_rootPath).arg("/lut_lin_tables.3d");
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
		double Kr, Kg, Kb;// _coefs[_currentCoef].x, Kg = _coefs[_currentCoef].y, Kb = _coefs[_currentCoef].z;

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

	return ok;*/
return true;
}
