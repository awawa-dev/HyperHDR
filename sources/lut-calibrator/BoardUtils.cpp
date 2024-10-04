/* BoardUtils.cpp
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

#include <lut-calibrator/BoardUtils.h>
#include <lut-calibrator/YuvConverter.h>
#include <utils-image/utils-image.h>
#include <iostream>
#include <fstream>
#include <QFile>

// YUV444
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,colorspace=space=bt709:primaries=bt709:range=pc:trc=iec61966-2-1:ispace=bt709:iprimaries=bt709:irange=pc:itrc=iec61966-2-1:format=yuv444p10:fast=0:dither=none -c:v libx265 -pix_fmt yuv444p10le -profile:v main444-10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:lossless=1:colorprim=bt709:transfer=iec61966-2-1:colormatrix=bt709:range=full -f mp4 test_SDR_yuv444_high_quality.mp4
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,zscale=m=2020_ncl:p=2020:t=smpte2084:r=full:min=709:pin=709:tin=iec61966-2-1:rin=full:c=topleft:npl=200,format=yuv420p10le -c:v libx265 -vtag hvc1 -pix_fmt yuv444p10le -profile:v main444-10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:hdr10=1:lossless=1:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:range=full -f mp4 test_HDR_yuv444_high_quality.mp4
// YUV420
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,colorspace=space=bt709:primaries=bt709:range=pc:trc=iec61966-2-1:ispace=bt709:iprimaries=bt709:irange=pc:itrc=iec61966-2-1:format=yuv444p12:fast=0:dither=none -c:v libx265 -pix_fmt yuv420p10le -profile:v main10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:lossless=1:colorprim=bt709:transfer=iec61966-2-1:colormatrix=bt709:range=full -f mp4 test_SDR_yuv420_low_quality.mp4
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,zscale=m=2020_ncl:p=2020:t=smpte2084:r=full:min=709:pin=709:tin=iec61966-2-1:rin=full:c=topleft:npl=200,format=yuv420p10le -c:v libx265 -vtag hvc1 -pix_fmt yuv420p10le -profile:v main10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:hdr10=1:lossless=1:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:range=full -f mp4 test_HDR_yuv420_low_quality.mp4

using namespace linalg;
using namespace aliases;

namespace BoardUtils
{

	int indexToColorAndPos(int index, byte3& color, int2& position)
	{
		int currentIndex = index % SCREEN_SAMPLES_PER_BOARD;
		int boardIndex = index / SCREEN_SAMPLES_PER_BOARD;

		position = int2(0, 1);		
		position.y += currentIndex / (SCREEN_BLOCKS_X / 2);
		position.x += (currentIndex % (SCREEN_BLOCKS_X / 2)) * 2 + ((position.y  + boardIndex  )% 2);

		int B = (index % SCREEN_COLOR_DIMENSION) * SCREEN_COLOR_STEP;
		int G = ((index / (SCREEN_COLOR_DIMENSION)) % SCREEN_COLOR_DIMENSION) * SCREEN_COLOR_STEP;
		int R = (index / (SCREEN_COLOR_DIMENSION * SCREEN_COLOR_DIMENSION)) * SCREEN_COLOR_STEP;

		color.x = std::min(R, 255);
		color.y = std::min(G, 255);
		color.z = std::min(B, 255);

		return boardIndex;
	}

	CapturedColor readBlock(const Image<ColorRgb>& yuvImage, int2 position, byte3* _color)
	{
		const double2 delta(yuvImage.width() / (double)SCREEN_BLOCKS_X, yuvImage.height() / (double)SCREEN_BLOCKS_Y);
		CapturedColor color;

		if (_color != nullptr)
			color.setSourceRGB(*_color);

		const double2 positionF{ position };
		const double2 startF = positionF * delta;
		const double2 endF = ((positionF + double2(1, 1)) * delta) - double2(1, 1);
		const int2 middle{ (startF + endF) / 2 };
		
		if (middle.x + 1 >= static_cast<int>(yuvImage.width()) || middle.y + 1 >= static_cast<int>(yuvImage.height()))
			throw std::runtime_error("Incorrect image size");

		for (int x = -1; x <= 1; x++)
			for (int y = -1; y <= 1; y++)
			{
				auto pos = middle + int2(x, y);
				color.addColor(yuvImage(pos.x, pos.y));
			}
		if (!color.calculateFinalColor())
			throw std::runtime_error("Too much noice while reading the color");
		return color;
	}

	void getWhiteBlackColorLevels(const Image<ColorRgb>& yuvImage, CapturedColor& white, CapturedColor& black, int& line)
	{
		auto top = readBlock(yuvImage, int2(0, 0));
		auto bottom = readBlock(yuvImage, int2(0, SCREEN_BLOCKS_Y - 1));
		if (top.y() > bottom.y())
		{
			white = top;
			black = bottom;
			line = 0;
		}
		else
		{
			white = bottom;
			black = top;
			line = SCREEN_BLOCKS_Y - 1;
		}
	}

	bool verifyBlackColorPattern(const Image<ColorRgb>& yuvImage, bool isFirstWhite, CapturedColor& black)
	{
		try
		{
			for (int x = 0; x < SCREEN_BLOCKS_X; x++)
				for (int y = 0; y < SCREEN_BLOCKS_Y; y++)
					if ((x + isFirstWhite + y) % 2 == 0)
					{
						auto test = readBlock(yuvImage, int2(x, y));
						if (test.Y() > black.Y() + SCREEN_MAX_CRC_BRIGHTNESS_ERROR)
							return false;
					}
		}
		catch (std::exception& ex)
		{
			// too much noice or too low resolution
			return false;
		}

		return true;
	}

	int readCrc(const Image<ColorRgb>& yuvImage, int line, CapturedColor& white)
	{
		int boardIndex = 0;
		int x = 0;

		for (int i = 0; i < SCREEN_CRC_COUNT; i++, x += 2)
		{
			auto test = readBlock(yuvImage, int2(x, line));
			if (test.Y() + SCREEN_MAX_CRC_BRIGHTNESS_ERROR < white.Y())
				throw std::runtime_error("Invalid CRC header");
		}

		while (true && x < SCREEN_BLOCKS_X)
		{
			auto test = readBlock(yuvImage, int2(x, line));
			if (test.Y() + SCREEN_MAX_CRC_BRIGHTNESS_ERROR < white.Y())
				break;
			boardIndex++;
			x += 2;
		}

		return boardIndex;
	}

	bool parseBoard(Logger* _log, const Image<ColorRgb>& yuvImage, int& boardIndex, CapturedColors& allColors, bool multiFrame)
	{
		bool exit = (multiFrame) ? false : true;

		int line = 0;
		CapturedColor white, black;

		try
		{
			getWhiteBlackColorLevels(yuvImage, white, black, line);
		}
		catch (std::exception& ex)
		{
			Error(_log, "Too much noice or too low resolution");
			return false;
		}

		if (!verifyBlackColorPattern(yuvImage, (line == 0), black))
		{
			Error(_log, "The black color pattern failed, too much noice or too low resolution");
			return false;
		}

		try
		{
			boardIndex = readCrc(yuvImage, line, white);
			if (boardIndex > SCREEN_LAST_BOARD_INDEX)
				throw std::runtime_error("Unexpected board");

		}
		catch (std::exception& ex)
		{
			Error(_log, "Too much noice or too low resolution");
			return false;
		}

		if (allColors.isCaptured(boardIndex))
		{
			return true;
		}

		int max1 = (boardIndex + 1) * SCREEN_SAMPLES_PER_BOARD;
		int max2 = std::pow(SCREEN_COLOR_DIMENSION, 3);
		for (int currentIndex = boardIndex * SCREEN_SAMPLES_PER_BOARD; currentIndex < max1 && currentIndex < max2; currentIndex++)
		{
			byte3 color;
			int2 position;
			indexToColorAndPos(currentIndex, color, position);

			int B = (currentIndex % SCREEN_COLOR_DIMENSION);
			int G = ((currentIndex / (SCREEN_COLOR_DIMENSION)) % SCREEN_COLOR_DIMENSION);
			int R = (currentIndex / (SCREEN_COLOR_DIMENSION * SCREEN_COLOR_DIMENSION));

			try
			{
				auto capturedColor = readBlock(yuvImage, position, &color);
				if (allColors.all[R][G][B].hasAnySample())
				{					
					allColors.all[R][G][B].importColors(capturedColor);
					if (allColors.all[R][G][B].hasAllSamples())
						exit = true;
				}
				else
				{
					capturedColor.setSourceRGB(color);
					allColors.all[R][G][B] = capturedColor;
				}
			}
			catch (std::exception& ex)
			{
				Error(_log, "Could not read position [%i, %i]. Too much noice or too low resolution", position.x, position.y);
				return false;
			}
		}

		if (black.Y() > SCREEN_YUV_RANGE_LIMIT || white.Y() < 255 - SCREEN_YUV_RANGE_LIMIT)
		{
			if (allColors.getRange() == YuvConverter::COLOR_RANGE::FULL)
				Error(_log, "The YUV range is changing. Now is LIMITED.");
			else if (allColors.getRange() == YuvConverter::COLOR_RANGE::UNKNOWN)
				Info(_log, "The YUV range is LIMITED");
			allColors.setRange(YuvConverter::COLOR_RANGE::LIMITED);
		}
		else
		{
			if (allColors.getRange() == YuvConverter::COLOR_RANGE::LIMITED)
				Error(_log, "The YUV range is changing. Now is FULL.");
			else if (allColors.getRange() == YuvConverter::COLOR_RANGE::UNKNOWN)
				Info(_log, "The YUV range is FULL");
			allColors.setRange(YuvConverter::COLOR_RANGE::FULL);
		}

		if (multiFrame)
		{
			if (!exit)
				Info(_log, "Successfully parsed image of test board no. %i. Waiting for another frame...", boardIndex);
			else
				Info(_log, "Successfully parsed final image of test board no. %i", boardIndex);
		}
		else
		{
			Info(_log, "Successfully parsed image of the %i test board", boardIndex);
		}

		return exit;
	}

	Image<ColorRgb> loadTestBoardAsYuv(const std::string& filename)
	{
		YuvConverter yuvConverter;
		auto image = utils_image::load2image(filename);
		for (int y = 0; y < static_cast<int>(image.height()); y++)
			for (int x = 0; x < static_cast<int>(image.width()); x++)
			{
				ColorRgb& inputRgb = image(x, y);
				const double3 scaledRgb = double3(inputRgb.red, inputRgb.green, inputRgb.blue) / 255.0;
				const double3 outputYuv = yuvConverter.toYuvBT709(YuvConverter::COLOR_RANGE::FULL, scaledRgb) * 255.0;
				const ColorRgb outputRgb = ColorRgb(ColorRgb::round(outputYuv.x), ColorRgb::round(outputYuv.y), ColorRgb::round(outputYuv.z));
				inputRgb = outputRgb;
			}
		return image;
	}

	void createTestBoards(const char* pattern)
	{
		const int2 margin(3, 2);
		int maxIndex = std::pow(SCREEN_COLOR_DIMENSION, 3);
		int boardIndex = 0;
		Image<ColorRgb> image(1920, 1080);
		const int2 delta(image.width() / SCREEN_BLOCKS_X, image.height() / SCREEN_BLOCKS_Y);

		auto saveImage = [](Image<ColorRgb>& image, const int2& delta, int boardIndex, const char* pattern, int2 margin)
			{
				for (int line = 0; line < SCREEN_BLOCKS_Y; line += SCREEN_BLOCKS_Y - 1)
				{
					int2 position = int2((line + boardIndex) % 2, line);

					for (int x = 0; x < boardIndex + SCREEN_CRC_COUNT; x++, position.x += 2)
					{
						const int2 start = position * delta;
						const int2 end = ((position + int2(1, 1)) * delta) - int2(1, 1);
						image.fastBox(start.x + margin.x, start.y + margin.y, end.x - margin.x, end.y - margin.y, 255, 255, 255);
					}
				}
				utils_image::savePng(QString(pattern).arg(QString::number(boardIndex)).toStdString(), image);
			};


		image.clear();

		if (256 % SCREEN_COLOR_STEP > 0 || image.width() % SCREEN_BLOCKS_X || image.height() % SCREEN_BLOCKS_Y)
			return;

		for (int index = 0; index < maxIndex; index++)
		{
			byte3 color;
			int2 position;
			int currentBoard = indexToColorAndPos(index, color, position);

			if (currentBoard < 0)
				return;

			if (boardIndex != currentBoard)
			{
				saveImage(image, delta, boardIndex, pattern, margin);
				image.clear();
				boardIndex = currentBoard;
			}

			const int2 start = position * delta;
			const int2 end = ((position + int2(1, 1)) * delta) - int2(1, 1);

			image.fastBox(start.x + margin.x, start.y + margin.y, end.x - margin.x, end.y - margin.y, color.x, color.y, color.z);
		}
		saveImage(image, delta, boardIndex, pattern, margin);
	}

	bool verifyTestBoards(Logger* _log, const char* pattern)
	{
		int boardIndex = -1;
		CapturedColors captured;

		for (int i = 0; i <= SCREEN_LAST_BOARD_INDEX; i++)
		{
			QString file = QString(pattern).arg(QString::number(i));
			Image<ColorRgb> image;

			if (!QFile::exists(file))
			{
				Error(_log, "LUT test board: %i. File %s does not exists", i, QSTRING_CSTR(file));
				return false;
			}

			image = loadTestBoardAsYuv(file.toStdString());

			if (image.width() == 1 || image.height() == 1)
			{
				Error(_log, "LUT test board: %i. Could load PNG: %s", i, QSTRING_CSTR(file));
				return false;
			}

			if (!parseBoard(_log, image, boardIndex, captured))
			{
				Error(_log, "LUT test board: %i. Could not parse: %s", i, QSTRING_CSTR(file));
				return false;
			}

			if (boardIndex != i)
			{
				Error(_log, "LUT test board: %i. Could not parse: %s. Incorrect board index: %i", i, QSTRING_CSTR(file), boardIndex);
				return false;
			}
			captured.setCaptured(boardIndex);
		}

		if (!captured.areAllCaptured())
		{
			Error(_log, "Not all test boards were loaded");
			return false;
		}

		// verify colors
		YuvConverter converter;
		int maxError = 0, totalErrors = 0;
		for (int r = 0; r < SCREEN_COLOR_DIMENSION; r++)
			for (int g = 0; g < SCREEN_COLOR_DIMENSION; g++)
				for (int b = 0; b < SCREEN_COLOR_DIMENSION; b++)
				{
					const auto& sample = captured.all[r][g][b];
					int3 sourceRgb = sample.getSourceRGB();
					auto result = converter.toRgb(YuvConverter::COLOR_RANGE::FULL, YuvConverter::YUV_COEFS::BT709, sample.yuv()) * 255.0;
					int3 outputRgb = ColorSpaceMath::to_int3(ColorSpaceMath::to_byte3(result));
					int distance = linalg::distance(sourceRgb, outputRgb);
					if (distance > maxError)
					{
						totalErrors++;
						maxError = distance;
						Warning(_log, "Current max error = %i for color (%i, %i, %i) received (%i, %i, %i)", maxError,
							sourceRgb.x, sourceRgb.y, sourceRgb.z,
							outputRgb.x, outputRgb.y, outputRgb.z);
					}
					else if (distance == maxError && distance > 0)
					{
						totalErrors++;
					}
				}
		if (maxError > 1)
		{
			Error(_log, "Failed to verify colors. The error is too high (%i > 1)", maxError);
			return false;
		}

		// all is OK
		Info(_log, "All [0 - %i] LUT test boards were tested successfully. Total small errors = %i", SCREEN_LAST_BOARD_INDEX, totalErrors);
		return true;
	}

	bool CapturedColors::isCaptured(int index) const
	{
		return  ((_capturedFlag & (1 << index)));
	}

	bool CapturedColors::areAllCaptured()
	{
		for (int i = 0; i <= BoardUtils::SCREEN_LAST_BOARD_INDEX; i++)
			if (!isCaptured(i))
				return false;

		finilizeBoard();

		return true;
	}

	void CapturedColors::finilizeBoard()
	{
		// update stats
		bool limited = (getRange() == YuvConverter::COLOR_RANGE::LIMITED);
		_yRange = (limited) ? (235.0 - 16.0) / 255.0 : 1.0;
		_yShift = (limited) ? (16.0 / 255.0) : 0;
		_downYLimit = all[0][0][0].y();
		_upYLimit = all[SCREEN_COLOR_DIMENSION - 1][SCREEN_COLOR_DIMENSION - 1][SCREEN_COLOR_DIMENSION - 1].y();
	}

	void CapturedColors::correctYRange(double3& yuv, double yRange, double upYLimit, double downYLimit, double yShift)
	{
		yuv.x = ((yuv.x - downYLimit) / (upYLimit - downYLimit)) * yRange + yShift;
	}

	void CapturedColors::getSignalParams(double& yRange, double& upYLimit, double& downYLimit, double& yShift)
	{
		yRange = _yRange;
		upYLimit = _upYLimit;
		downYLimit = _downYLimit;
		yShift = _yShift;
	}

	void CapturedColors::setCaptured(int index)
	{
		_capturedFlag |= (1 << index);
	}

	void CapturedColors::setRange(YuvConverter::COLOR_RANGE range)
	{
		_range = range;
	}

	YuvConverter::COLOR_RANGE CapturedColors::getRange() const
	{
		return _range;
	}

	bool CapturedColors::saveResult(const char* filename, const std::string& result)
	{
		std::ofstream myfile;
		myfile.open(filename, std::ios::trunc | std::ios::out);

		if (!myfile.is_open())
			return false;

		std::list<std::pair<const char*, const char*>> arrayMark = { {"{", "}"} }; //  {"np.array([", "])"},

		myfile << result << std::endl;

		for(const auto& currentArray : arrayMark)
		{
			myfile << "capturedData = " << currentArray.first;
			for (int r = 0; r < SCREEN_COLOR_DIMENSION; r++)
			{
				for (int g = 0; g < SCREEN_COLOR_DIMENSION; g++)
				{
					myfile  << std::endl << "\t";
					for (int b = 0; b < SCREEN_COLOR_DIMENSION; b++)
					{
						auto elems = all[r][g][b].getInputYUVColors();
						myfile << currentArray.first;

						for (const auto& elem : elems)
						{
							if (elem != elems.front())
							{
								myfile << ",";
							}

							myfile << " " << std::to_string(elem.second) << ",";
							myfile << std::to_string((int)elem.first.x) << ",";
							myfile << std::to_string((int)elem.first.y) << ",";
							myfile << std::to_string((int)elem.first.z);
						}

						myfile << currentArray.second << ((r + 1 < SCREEN_COLOR_DIMENSION || g + 1 < SCREEN_COLOR_DIMENSION || b + 1 < SCREEN_COLOR_DIMENSION) ? "," : "");
					}
				}
			}
			myfile << std::endl << currentArray.second << ";" << std::endl;
		}

		myfile.close();

		return true;
	}
}

