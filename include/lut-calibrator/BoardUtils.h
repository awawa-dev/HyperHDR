#pragma once

/* BoardUtils.h
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
	#include <QString>
	#include <cmath>
	#include <cstring>
#endif

#include <linalg.h>
#include <lut-calibrator/ColorSpace.h>
#include <image/ColorRgb.h>
#include <image/Image.h>
#include <lut-calibrator/CapturedColor.h>
#include <utils/Logger.h>
#include <lut-calibrator/YuvConverter.h>

namespace BoardUtils
{
	class CapturedColors;
	const int SCREEN_BLOCKS_X = 48;
	const int SCREEN_BLOCKS_Y = 30;
	const int SCREEN_COLOR_STEP = 16;
	const int SCREEN_COLOR_DIMENSION = (256 / SCREEN_COLOR_STEP) + 1;

	const int IMPORT_SCALE = 1000000;

	const int SCREEN_YUV_RANGE_LIMIT = 2;

	const int SCREEN_CRC_LINES = 2;
	const int SCREEN_CRC_COUNT = 5;
	const int SCREEN_MAX_CRC_BRIGHTNESS_ERROR = 1;
	const int SCREEN_MAX_COLOR_NOISE_ERROR = 8;
	const int SCREEN_SAMPLES_PER_BOARD = (SCREEN_BLOCKS_X / 2) * (SCREEN_BLOCKS_Y - SCREEN_CRC_LINES);
	const int SCREEN_LAST_BOARD_INDEX = std::pow(SCREEN_COLOR_DIMENSION, 3) / SCREEN_SAMPLES_PER_BOARD;

	int indexToColorAndPos(int index, byte3& color, int2& position);
	CapturedColor readBlock(const Image<ColorRgb>& yuvImage, int2 position, byte3* _color = nullptr);
	void getWhiteBlackColorLevels(const Image<ColorRgb>& yuvImage, CapturedColor& white, CapturedColor& black, int& line);
	bool verifyBlackColorPattern(const Image<ColorRgb>& yuvImage, bool isFirstWhite, CapturedColor& black);
	bool parseBoard(Logger* _log, const Image<ColorRgb>& yuvImage, int& boardIndex, CapturedColors& allColors, bool multiFrame = false);
	Image<ColorRgb> loadTestBoardAsYuv(const std::string& filename);
	void createTestBoards(const char* pattern = "D:/table_%1.png");
	bool verifyTestBoards(Logger* _log, const char* pattern = "D:/table_%1.png");

	class CapturedColors
	{
	private:
		int _capturedFlag = 0;
		YuvConverter::COLOR_RANGE _range = YuvConverter::COLOR_RANGE::UNKNOWN;

		double _yRange = 0;
		double _yShift = 0;
		double _downYLimit = 0;
		double _upYLimit = 0;

	public:
		CapturedColors() = default;

		std::vector<std::vector<std::vector<CapturedColor>>> all = std::vector(BoardUtils::SCREEN_COLOR_DIMENSION,
			std::vector<std::vector<CapturedColor>>(BoardUtils::SCREEN_COLOR_DIMENSION,
				std::vector <CapturedColor>(BoardUtils::SCREEN_COLOR_DIMENSION)));;

		bool isCaptured(int index) const;
		bool areAllCaptured();
		void finilizeBoard();
		static void correctYRange(double3& yuv, double yRange, double upYLimit, double downYLimit, double yShift);
		void getSignalParams(double& yRange, double& upYLimit, double& downYLimit, double& yShift);
		void setCaptured(int index);
		void setRange(YuvConverter::COLOR_RANGE range);
		YuvConverter::COLOR_RANGE getRange() const;
		bool saveResult(const char* filename = "D:/result.txt", const std::string& result = "");
	};
};
